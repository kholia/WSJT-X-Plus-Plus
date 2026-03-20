#ifndef SIMPLE_CAT_TRANSCEIVER_HPP__
#define SIMPLE_CAT_TRANSCEIVER_HPP__

#include <QString>

#include <QtSerialPort/QSerialPort>

#include "PollingTransceiver.hpp"
#include "TransceiverFactory.hpp"

class SimpleCATTransceiver final : public PollingTransceiver {
  Q_OBJECT

public:
  static void register_transceivers(logger_type *,
                                    TransceiverFactory::Transceivers *,
                                    unsigned id);

  explicit SimpleCATTransceiver(logger_type *,
                                TransceiverFactory::ParameterPack const &,
                                QObject *parent = nullptr);

private:
  int do_start() override;
  void do_stop() override;
  void do_frequency(Frequency, MODE, bool no_ignore) override;
  void do_tx_frequency(Frequency, MODE, bool no_ignore) override;
  void do_mode(MODE) override;
  void do_ptt(bool) override;
  void do_trfrequency(double) override;
  void do_tx_symbols(QString const &) override;
  void do_modulator_start(QString, unsigned, double, double, double, bool, bool,
                          double, double) override;
  void do_modulator_stop(bool) override;
  void do_poll() override;

  QString send_command(QString const &command, int timeout_ms = 1000);
  void send_ok_command(QString const &command,
                       QString const &expected_prefix = "OK");
  void refresh_state();
  QString map_mode(MODE) const;
  MODE parse_mode(QString const &) const;
  void apply_serial_settings();
  void ensure_modulator_ready();
  void maybe_finish_deferred_ptt();

  TransceiverFactory::ParameterPack params_;
  QSerialPort port_;
  QString pending_tx_symbols_;
  QString pending_digital_mode_{"FT8"};
  quint32 pending_offset_hz_{1500u};
  bool ptt_requested_{false};
  bool modulator_ready_{false};
  bool tx_active_{false};
};

#endif
