#include "SimpleCATTransceiver.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>

#include "moc_SimpleCATTransceiver.cpp"

namespace {
char const *const simple_cat_transceiver_name{"Simple CAT"};
}

void SimpleCATTransceiver::register_transceivers(
    logger_type *, TransceiverFactory::Transceivers *registry, unsigned id) {
  (*registry)[simple_cat_transceiver_name] = TransceiverFactory::Capabilities{
      id, TransceiverFactory::Capabilities::serial, true};
}

SimpleCATTransceiver::SimpleCATTransceiver(
    logger_type *logger, TransceiverFactory::ParameterPack const &params,
    QObject *parent)
    : PollingTransceiver{logger, params.poll_interval, parent},
      params_{params} {}

int SimpleCATTransceiver::do_start() {
  CAT_TRACE("Simple CAT start on" << params_.serial_port);
  port_.setPortName(params_.serial_port);
  apply_serial_settings();

  if (!port_.open(QIODevice::ReadWrite)) {
    throw error{tr("Simple CAT: cannot open serial port %1: %2")
                    .arg(params_.serial_port, port_.errorString())};
  }

  QThread::msleep(250);
  port_.clear();
  try {
    send_ok_command("HELP");
  } catch (std::exception const &e) {
    CAT_TRACE("Simple CAT optional HELP probe failed:" << e.what());
  }
  refresh_state();
  return 0;
}

void SimpleCATTransceiver::do_stop() {
  CAT_TRACE("Simple CAT stop" << "ptt_requested=" << ptt_requested_
                              << "modulator_ready=" << modulator_ready_
                              << "tx_active=" << tx_active_);
  ptt_requested_ = false;
  modulator_ready_ = false;
  tx_active_ = false;
  pending_tx_symbols_.clear();
  if (port_.isOpen()) {
    port_.close();
  }
}

void SimpleCATTransceiver::do_frequency(Frequency f, MODE m, bool) {
  send_ok_command(QString{"FREQ %1"}.arg(f));
  update_rx_frequency(f);
  update_other_frequency(0);
  update_split(false);
  if (m != UNK) {
    do_mode(m);
  }
}

void SimpleCATTransceiver::do_tx_frequency(Frequency f, MODE, bool) {
  update_other_frequency(f);
  update_split(f != 0);
}

void SimpleCATTransceiver::do_mode(MODE mode) {
  auto command_mode = map_mode(mode);
  if (command_mode.isEmpty()) {
    return;
  }

  send_ok_command(QString{"MODE %1"}.arg(command_mode));
  update_mode(mode);
}

void SimpleCATTransceiver::do_ptt(bool on) {
  CAT_TRACE("Simple CAT do_ptt"
            << "on=" << on << "modulator_ready=" << modulator_ready_
            << "tx_active=" << tx_active_
            << "symbols_empty=" << pending_tx_symbols_.isEmpty());
  ptt_requested_ = on;
  if (!on) {
    modulator_ready_ = false;
    send_ok_command(tx_active_ ? "TX OFF" : "PTT OFF");
    tx_active_ = false;
    pending_tx_symbols_.clear();
    update_PTT(false);
    return;
  }

  if (modulator_ready_ || !pending_tx_symbols_.isEmpty()) {
    ensure_modulator_ready();
    maybe_finish_deferred_ptt();
  } else {
    send_ok_command("PTT ON");
    update_PTT(true);
  }
}

void SimpleCATTransceiver::do_trfrequency(double trfrequency) {
  pending_offset_hz_ = qMax(0, qRound(trfrequency));
}

void SimpleCATTransceiver::do_tx_symbols(QString const &tx_symbols) {
  auto simplified = tx_symbols.simplified();
  if (simplified.isEmpty() &&
      (ptt_requested_ || tx_active_ || modulator_ready_)) {
    CAT_TRACE(
        "Simple CAT ignoring empty tx_symbols while TX is active/pending");
    return;
  }

  pending_tx_symbols_ = simplified;
  CAT_TRACE("Simple CAT tx_symbols length=" << pending_tx_symbols_.size()
                                            << "value=" << pending_tx_symbols_);
}

void SimpleCATTransceiver::do_modulator_start(QString mode, unsigned, double,
                                              double frequency, double, bool,
                                              bool, double, double) {
  pending_digital_mode_ = mode.contains("FT4") ? "FT4" : "FT8";
  pending_offset_hz_ = qMax(0, qRound(frequency));
  CAT_TRACE("Simple CAT modulator_start"
            << "mode=" << pending_digital_mode_
            << "offset=" << pending_offset_hz_
            << "symbols_empty=" << pending_tx_symbols_.isEmpty()
            << "ptt_requested=" << ptt_requested_);
  ensure_modulator_ready();
  maybe_finish_deferred_ptt();
}

void SimpleCATTransceiver::do_modulator_stop(bool) {
  CAT_TRACE("Simple CAT modulator_stop" << "tx_active=" << tx_active_
                                        << "ptt_requested=" << ptt_requested_);
  if (tx_active_ || ptt_requested_ || modulator_ready_) {
    send_ok_command("ABORT");
    tx_active_ = false;
    update_PTT(false);
  }
  modulator_ready_ = false;
  ptt_requested_ = false;
  pending_tx_symbols_.clear();
}

void SimpleCATTransceiver::do_poll() { refresh_state(); }

QString SimpleCATTransceiver::send_command(QString const &command,
                                           int timeout_ms) {
  if (!port_.isOpen()) {
    throw error{tr("Simple CAT: serial port is not open")};
  }

  CAT_TRACE("Simple CAT >>" << command);
  port_.clear(QSerialPort::Input);

  auto payload = command.toUtf8();
  payload.append('\n');

  if (port_.write(payload) != payload.size()) {
    throw error{tr("Simple CAT: failed to write command %1").arg(command)};
  }
  if (!port_.waitForBytesWritten(timeout_ms)) {
    throw error{tr("Simple CAT: timeout writing command %1").arg(command)};
  }

  QByteArray response;
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeout_ms) {
    if (port_.canReadLine()) {
      response += port_.readLine();
      break;
    }

    auto remaining = timeout_ms - static_cast<int>(timer.elapsed());
    if (remaining <= 0 || !port_.waitForReadyRead(remaining)) {
      continue;
    }
    response += port_.readAll();
    if (response.contains('\n')) {
      break;
    }
  }

  auto line = QString::fromUtf8(response).trimmed();
  if (line.isEmpty()) {
    throw error{tr("Simple CAT: no response to command %1").arg(command)};
  }
  if (line.startsWith("ERR")) {
    CAT_TRACE("Simple CAT <<" << line);
    throw error{tr("Simple CAT: %1").arg(line)};
  }
  CAT_TRACE("Simple CAT <<" << line);
  return line;
}

void SimpleCATTransceiver::send_ok_command(QString const &command,
                                           QString const &expected_prefix) {
  auto response = send_command(command);
  if (!response.startsWith(expected_prefix)) {
    throw error{
        tr("Simple CAT: unexpected response to %1: %2").arg(command, response)};
  }
}

void SimpleCATTransceiver::refresh_state() {
  auto freq_response = send_command("FREQ");
  auto mode_response = send_command("MODE");
  auto ptt_response = send_command("PTT");
  QString parsed_mode{"UNK"};
  quint64 parsed_frequency{0};
  bool parsed_ptt{false};

  auto freq_parts = freq_response.split(' ', Qt::SkipEmptyParts);
  if (freq_parts.size() == 2) {
    bool ok = false;
    auto frequency = freq_parts[1].toULongLong(&ok);
    if (ok) {
      parsed_frequency = frequency;
      update_rx_frequency(frequency);
      update_other_frequency(0);
      update_split(false);
    }
  }

  auto mode_parts = mode_response.split(' ', Qt::SkipEmptyParts);
  if (mode_parts.size() == 2) {
    parsed_mode = mode_parts[1];
    update_mode(parse_mode(mode_parts[1]));
  }

  auto ptt_parts = ptt_response.split(' ', Qt::SkipEmptyParts);
  if (ptt_parts.size() == 2) {
    auto ptt_on = ptt_parts[1] == "ON";
    parsed_ptt = ptt_on;
    if (!ptt_on) {
      tx_active_ = false;
    }
    update_PTT(ptt_on);
  }

  CAT_TRACE("Simple CAT poll" << "freq=" << parsed_frequency
                              << "mode=" << parsed_mode << "ptt=" << parsed_ptt
                              << "tx_active=" << tx_active_);
}

QString SimpleCATTransceiver::map_mode(MODE mode) const {
  switch (mode) {
  case USB:
  case DIG_U:
    return "USB";
  case LSB:
  case DIG_L:
    return "LSB";
  case AM:
    return "AM";
  case FM:
  case DIG_FM:
    return "FM";
  default:
    return {};
  }
}

auto SimpleCATTransceiver::parse_mode(QString const &mode) const -> MODE {
  if (mode == "USB") {
    return USB;
  }
  if (mode == "LSB") {
    return LSB;
  }
  if (mode == "AM") {
    return AM;
  }
  if (mode == "FM") {
    return FM;
  }
  return UNK;
}

void SimpleCATTransceiver::apply_serial_settings() {
  port_.setBaudRate(params_.baud);

  switch (params_.data_bits) {
  case TransceiverFactory::seven_data_bits:
    port_.setDataBits(QSerialPort::Data7);
    break;
  case TransceiverFactory::eight_data_bits:
  case TransceiverFactory::default_data_bits:
  default:
    port_.setDataBits(QSerialPort::Data8);
    break;
  }

  switch (params_.stop_bits) {
  case TransceiverFactory::two_stop_bits:
    port_.setStopBits(QSerialPort::TwoStop);
    break;
  case TransceiverFactory::one_stop_bit:
  case TransceiverFactory::default_stop_bits:
  default:
    port_.setStopBits(QSerialPort::OneStop);
    break;
  }

  switch (params_.handshake) {
  case TransceiverFactory::handshake_hardware:
    port_.setFlowControl(QSerialPort::HardwareControl);
    break;
  case TransceiverFactory::handshake_XonXoff:
    port_.setFlowControl(QSerialPort::SoftwareControl);
    break;
  case TransceiverFactory::handshake_none:
  case TransceiverFactory::handshake_default:
  default:
    port_.setFlowControl(QSerialPort::NoFlowControl);
    break;
  }

  port_.setParity(QSerialPort::NoParity);
}

void SimpleCATTransceiver::ensure_modulator_ready() {
  if (modulator_ready_) {
    return;
  }

  send_ok_command(QString{"DIGMODE %1"}.arg(pending_digital_mode_));
  send_ok_command(QString{"OFFSET %1"}.arg(pending_offset_hz_));
  if (!pending_tx_symbols_.isEmpty()) {
    send_ok_command(QString{"ITONE %1"}.arg(pending_tx_symbols_));
  }

  modulator_ready_ = true;
  CAT_TRACE("Simple CAT ensure_modulator_ready"
            << "mode=" << pending_digital_mode_
            << "offset=" << pending_offset_hz_
            << "symbols_empty=" << pending_tx_symbols_.isEmpty());
}

void SimpleCATTransceiver::maybe_finish_deferred_ptt() {
  CAT_TRACE("Simple CAT maybe_finish_deferred_ptt"
            << "ptt_requested=" << ptt_requested_ << "modulator_ready="
            << modulator_ready_ << "tx_active=" << tx_active_);
  if (ptt_requested_ && modulator_ready_ && !tx_active_) {
    send_ok_command("TX ON");
    tx_active_ = true;
    update_PTT(true);
  } else if (ptt_requested_ && modulator_ready_ && tx_active_) {
    CAT_TRACE("Simple CAT skipping duplicate TX ON");
  }
}
