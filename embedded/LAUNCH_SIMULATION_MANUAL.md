# Manual da Simulacao de Lancamento Apollo 11

Este manual explica como rodar a simulacao de lancamento usando:

- `ESP32`: AGC core e simulador de ascento
- `ESP8266`: DSKY slave com interface web
- navegador no PC/celular: botoes e displays do DSKY
- serial USB do ESP32: monitoramento limpo da missao

Importante: esta simulacao ainda nao e o software Comanche real executando uma rope completa. Ela e um `Launch Monitor`: uma camada de missao que faz o DSKY passar por eventos principais do ascento Apollo 11 enquanto o AGC core embarcado continua rodando.

## Arquivos usados

- `embedded/esp32_agc_core/esp32_agc_core.ino`: AGC core no ESP32 e simulacao de lancamento
- `embedded/esp8266_dsky_slave/esp8266_dsky_slave.ino`: DSKY slave no ESP8266
- `embedded/esp8266_dsky_slave/web_dsky_page.h`: pagina web servida pelo ESP8266
- `embedded/esp8266_dsky_slave/wifi_config.h.example`: modelo opcional para conectar o ESP8266 na sua rede Wi-Fi

## Ligacao entre ESP32 e ESP8266

Use uma UART dedicada entre as placas.

```text
ESP32 GPIO17 / TX2  ->  ESP8266 GPIO14 / NodeMCU D5
ESP32 GPIO16 / RX2  <-  ESP8266 GPIO12 / NodeMCU D6
ESP32 GND           ->  ESP8266 GND
```

As duas placas trabalham em `3.3V`, entao nao use sinal UART de `5V` nessa ligacao.

Velocidades:

- UART entre placas: `38400`
- USB serial do ESP32: `115200`
- USB serial do ESP8266: `115200`

## Preparacao das placas

1. Abra `embedded/esp8266_dsky_slave/esp8266_dsky_slave.ino` na Arduino IDE.
2. Selecione sua placa ESP8266 e envie o sketch.
3. Abra `embedded/esp32_agc_core/esp32_agc_core.ino` na Arduino IDE.
4. Selecione sua placa ESP32 e envie o sketch.
5. Conecte a UART entre as placas usando o pinout acima.
6. Abra o monitor serial do ESP32 em `115200`.
7. Abra o monitor serial do ESP8266 em `115200`.

## Wi-Fi do DSKY

Sem configuracao extra, o ESP8266 cria uma rede propria:

```text
SSID: AGC-DSKY
Senha: apollo11
URL: http://192.168.4.1
```

Conecte o PC ou celular nessa rede e abra `http://192.168.4.1`.

Para conectar o ESP8266 na sua rede Wi-Fi, crie um arquivo local chamado `embedded/esp8266_dsky_slave/wifi_config.h` com este formato:

```cpp
#pragma once

#define DSKY_WIFI_SSID "NomeDaSuaRede"
#define DSKY_WIFI_PASSWORD "SenhaDaSuaRede"

#define DSKY_AP_SSID "AGC-DSKY"
#define DSKY_AP_PASSWORD "apollo11"
```

Esse arquivo esta no `.gitignore`, entao sua senha nao sera enviada para o GitHub. Quando conectar na sua rede, o ESP8266 imprime o IP no monitor serial. Ele tambem tenta responder em `http://agc-dsky.local`.

## Inicio rapido

No Web DSKY, pressione:

```text
VERB 37 NOUN 11 ENTR
```

Em botoes:

```text
VERB
3
7
NOUN
1
1
ENTR
```

O DSKY deve mudar para:

```text
P11 V16 N62
```

Durante a simulacao:

- `R1`: tempo de missao em segundos
- `R2`: altitude aproximada em quilometros
- `R3`: velocidade aproximada em metros por segundo

O tempo negativo em `R1` representa a contagem regressiva final. Por exemplo, `-00010` significa `T-10`.

## Parar a simulacao

No Web DSKY:

```text
VERB 37 NOUN 00 ENTR
```

Em botoes:

```text
VERB
3
7
NOUN
0
0
ENTR
```

## Rodar em tempo real

Por padrao, a simulacao roda em `x20`, entao o ascento ate a orbita de estacionamento dura cerca de 36 segundos.

Para rodar em tempo real pelo DSKY:

```text
VERB 37 NOUN 12 ENTR
```

Em botoes:

```text
VERB
3
7
NOUN
1
2
ENTR
```

## Comandos pelo serial do ESP32

No monitor serial do ESP32, em `115200`, voce pode usar:

```text
LAUNCH
LAUNCH,STOP
LAUNCH,STATUS
LAUNCH,SPEED,20
LAUNCH,REALTIME
```

Significado:

- `LAUNCH`: inicia a simulacao no modo acelerado atual
- `LAUNCH,STOP`: para e volta o painel para `P00 V16 N36`
- `LAUNCH,STATUS`: imprime o estado atual da simulacao
- `LAUNCH,SPEED,20`: define escala de tempo, de `1` a `100`
- `LAUNCH,REALTIME`: define escala `x1`

## Saida serial esperada no ESP32

Com a simulacao rodando, a saida limpa do ESP32 passa a incluir um bloco `ASC`:

```text
AGC P11 V16 N62 | R1 +00013 R2 +00002 R3 +00081 | ALM 0000 | JOY +0000,+0000,0 | Z 4000 A 21521 | CYC 123456 | RUN | ASC T+00:13 ROLL PROGRAM ALT 2km VEL 81m/s x20
```

Eventos importantes tambem aparecem como linhas `LAUNCH`:

```text
LAUNCH T+00:00 LIFTOFF | P11 V16 N62 | R1 T+0 R2 ALT_KM 0 R3 VEL_MS 0 | x20
LAUNCH T+01:23 MAX-Q | P11 V16 N62 | R1 T+83 R2 ALT_KM 14 R3 VEL_MS 520 | x20
LAUNCH T+11:45 PARKING ORBIT | P11 V16 N62 | R1 T+705 R2 ALT_KM 185 R3 VEL_MS 7800 | x20 COMPLETE
```

## Linha do tempo simulada

Estes sao os eventos atualmente codificados no ESP32:

| Tempo | Evento no serial |
| --- | --- |
| `T-00:10` | `TERMINAL COUNT` |
| `T-00:08` | `F-1 IGNITION` |
| `T+00:00` | `LIFTOFF` |
| `T+00:13` | `ROLL PROGRAM` |
| `T+00:34` | `ROLL COMPLETE` |
| `T+01:23` | `MAX-Q` |
| `T+02:17` | `S-IC INBOARD CUTOFF` |
| `T+02:44` | `S-IC/S-II STAGING` |
| `T+03:13` | `INTERSTAGE SEP` |
| `T+03:17` | `LES JETTISON` |
| `T+05:27` | `S-IVB TO COI` |
| `T+07:42` | `S-II INBOARD CUTOFF` |
| `T+08:22` | `MIXTURE SHIFT` |
| `T+09:00` | `ABORT MODE IV` |
| `T+09:15` | `S-II/S-IVB STAGING` |
| `T+11:45` | `PARKING ORBIT` |

As altitudes e velocidades sao aproximacoes para visualizacao no DSKY. Elas nao sao uma simulacao fisica completa do Saturn V.

## O que observar no Web DSKY

Ao iniciar:

- `PROG` deve virar `11`
- `VERB` deve virar `16`
- `NOUN` deve virar `62`
- `R1` comeca perto de `-00010`
- `R2` e `R3` comecam em `+00000`

Durante o ascento:

- `R1` aumenta ate `+00705`
- `R2` sobe ate aproximadamente `+00185`
- `R3` sobe ate aproximadamente `+07800`
- algumas lampadas podem acender para indicar atividade de programa, uplink/telemetria e tracking

Ao terminar:

- o serial mostra `PARKING ORBIT`
- o DSKY continua em `P11 V16 N62`
- `R1` fica em `+00705`
- `R2` fica perto de `+00185`
- `R3` fica perto de `+07800`

## Troubleshooting

Se o Web DSKY abre mas nao muda estado:

- confirme que o ESP32 e o ESP8266 tem `GND` em comum
- confirme `ESP32 GPIO17 -> ESP8266 D5/GPIO14`
- confirme `ESP8266 D6/GPIO12 -> ESP32 GPIO16`
- confira se os dois sketches foram enviados depois das ultimas alteracoes
- abra o serial do ESP8266 e procure linhas `DSKY LINK=1`

Se o Web DSKY nao abre:

- conecte na rede `AGC-DSKY`
- abra `http://192.168.4.1`
- se estiver usando sua rede Wi-Fi, veja o IP impresso no serial do ESP8266
- tente `http://agc-dsky.local` se seu sistema suportar mDNS

Se o ESP32 nao mostra linhas `LAUNCH`:

- abra o serial do ESP32 em `115200`
- envie `LAUNCH,STATUS`
- envie `LAUNCH`
- confirme que a saida USB nao esta em modo quiet com `USB,CLEAN`

Se digitar numeros no DSKY aciona alarme:

- primeiro pressione `VERB` ou `NOUN`
- depois digite exatamente dois digitos
- para iniciar a simulacao, a sequencia correta e `VERB 37 NOUN 11 ENTR`

## Fontes historicas usadas

- [Apollo 11 Flight Journal, Day 1: Launch](https://www.apollojournals.org/afj/ap11fj/01launch.html)
- [NASA Apollo 11 Mission Overview](https://www.nasa.gov/history/apollo-11-mission-overview/)
