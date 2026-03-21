# Simple CAT Protocol Specification

## Overview
Simple CAT is a text-based serial protocol for controlling amateur radio transceivers, designed for WSJT-X digital mode operation (FT8/FT4 and others).

## Physical Layer
| Parameter | Value |
|-----------|-------|
| Interface | USB CDC ACM (Virtual COM) |
| Baud Rate | 115200 (default), 1200 (bootloader) |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Line Ending | CR (`\r`) or LF (`\n`) |

## Command Format
```
<COMMAND> [ARGUMENT] <CR>
```
- Commands are case-insensitive
- Arguments are space-separated
- Response terminated with `\n`

## Commands

### Frequency Control
| Command | Response | Description |
|---------|----------|-------------|
| `FREQ` | `FREQ <Hz>` | Get current frequency (Hz) |
| `FREQ <Hz>` | `OK FREQ <Hz>` | Set frequency (e.g., `FREQ 14074000`) |

### Mode Control
| Command | Response | Description |
|---------|----------|-------------|
| `MODE` | `MODE <mode>` | Get current mode |
| `MODE <mode>` | `OK MODE <mode>` | Set mode: `USB`, `LSB`, `AM`, `FM` |

### Digital Mode (FT8/FT4)
| Command | Response | Description |
|---------|----------|-------------|
| `DIGMODE` | `DIGMODE <mode>` | Get digital mode |
| `DIGMODE <mode>` | `OK DIGMODE <mode>` | Set: `FT8` or `FT4` |

### TX Offset
| Command | Response | Description |
|---------|----------|-------------|
| `OFFSET` | `OFFSET <Hz>` | Get TX offset (Hz) |
| `OFFSET <Hz>` | `OK OFFSET <Hz>` | Set offset (e.g., `OFFSET 1200`) |

### PTT/TX Control
| Command | Response | Description |
|---------|----------|-------------|
| `PTT` | `PTT ON`/`PTT OFF` | Get PTT state |
| `PTT ON` | `OK PTT ON` | Enable PTT |
| `PTT OFF` | `OK PTT OFF` | Disable PTT |
| `TX ON` | `OK TX ON` | Start transmission |
| `TX OFF` | `OK TX OFF` | Stop transmission |
| `ABORT` | `OK ABORT` | Abort ongoing TX |

### ITONE (Symbol Data)
| Command | Response | Description |
|---------|----------|-------------|
| `ITONE <s...>` | `OK ITONE <n>` | Load symbols ('tones') |
| `ITONEHASH` | `ITONEHASH <md5>` | Get MD5 of loaded symbols |

**ITONE Format:** Space-separated symbols (each digit 0-9)
```
ITONE 0 0 1 3 2 1 0 3 3 1 1 2...
```
Or packed (firmware expands each character):
```
ITONE 0 0132 10331123303131023...
```

### Help
| Command | Response |
|---------|----------|
| `HELP` | Lists all supported commands |

## Response Format
- Success: `OK <COMMAND> [VALUE]\n`
- Error: `ERR <reason>\n`

## Example Session
```
FREQ
→ FREQ 14074000
FREQ 14074500
→ OK FREQ 14074500
MODE USB
→ OK MODE USB
DIGMODE FT4
→ OK DIGMODE FT4
OFFSET 1200
→ OK OFFSET 1200
ITONE 0 0 1 3 2 1 0 3 3...
→ OK ITONE 105
ITONEHASH
→ ITONEHASH 64d1ac9cfe09a9f5c401d63f180da386
TX ON
→ OK TX ON
TX OFF
→ OK TX OFF
```

## Error Codes
| Error | Description |
|-------|-------------|
| `ERR invalid frequency` | Frequency out of range |
| `ERR invalid mode` | Unknown mode string |
| `ERR invalid itone value` | Symbol not 0-9 |
| `ERR too many itones` | >255 symbols |
| `ERR command too long` | Line exceeds buffer |
| `ERR unsupported command` | Unknown command |
