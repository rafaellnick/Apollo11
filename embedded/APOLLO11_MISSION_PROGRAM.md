# Apollo 11 Mission Program Commands

Este arquivo descreve a camada `Apollo 11 Mission Program` do ESP32.

Ela nao substitui o Comanche real ainda. O objetivo desta camada e dar ao DSKY comandos de missao para todas as situacoes principais da Apollo 11 enquanto continuamos evoluindo o AGC core, a rope loader e os modelos de perifericos.

## Como executar pelo Web DSKY

Use sempre este formato:

```text
VERB 37 NOUN xx ENTR
```

Onde `xx` e o codigo da situacao de missao.

Exemplo para iniciar o lancamento acelerado:

```text
VERB 37 NOUN 11 ENTR
```

Exemplo para selecionar pouso lunar:

```text
VERB 37 NOUN 28 ENTR
```

## Como executar pelo serial do ESP32

Abra o monitor serial do ESP32 em `115200`.

Comandos:

```text
APOLLO11,LIST
APOLLO11,STATUS
APOLLO11,STOP
APOLLO11,28
MISSION,28
```

`APOLLO11,<noun>` e `MISSION,<noun>` fazem a mesma coisa.

## Leitura do DSKY

Para as situacoes estaticas, o DSKY mostra:

- `PROG`: programa AGC aproximado para aquela fase
- `VERB`: normalmente `16`, monitoramento
- `NOUN`: codigo da situacao selecionada
- `R1`: tempo GET em minutos, ou campo especial quando indicado
- `R2`: altitude, distancia, tempo de queima ou parametro da fase
- `R3`: velocidade, delta-v ou codigo de status

Para o lancamento `N11` ou `N12`, o display muda para `P11 V16 N62`:

- `R1`: tempo em segundos desde liftoff, negativo antes de `T+00:00`
- `R2`: altitude aproximada em km
- `R3`: velocidade aproximada em m/s

## Tabela de comandos DSKY

| Comando | Situacao | Display esperado | R1 | R2 | R3 |
| --- | --- | --- | --- | --- | --- |
| `V37 N00 ENTR` | parar programa de missao | `P00 V16 N36` | contador AGC | A | Z |
| `V37 N11 ENTR` | lancamento e ascento, acelerado `x20` | `P11 V16 N62` | `T+ sec` | `ALT_KM` | `VEL_MS` |
| `V37 N12 ENTR` | lancamento e ascento, tempo real `x1` | `P11 V16 N62` | `T+ sec` | `ALT_KM` | `VEL_MS` |
| `V37 N20 ENTR` | orbita terrestre de estacionamento | `P11 V16 N20` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N21 ENTR` | injecao translunar | `P15 V16 N21` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N22 ENTR` | transposicao e docking CSM/LM | `P17 V16 N22` | `GET_MIN` | `RANGE_M` | `DOCKED` |
| `V37 N23 ENTR` | costa translunar | `P23 V16 N23` | `GET_MIN` | `DIST_KKM` | `MCC` |
| `V37 N24 ENTR` | insercao em orbita lunar | `P40 V16 N24` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N25 ENTR` | orbita lunar | `P20 V16 N25` | `GET_MIN` | `ALT_KM` | `ORBIT` |
| `V37 N26 ENTR` | descida propulsada | `P63 V16 N26` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N27 ENTR` | fase final de pouso | `P66 V16 N27` | `GET_MIN` | `ALT_M` | `VEL_MS` |
| `V37 N28 ENTR` | pousado na superficie | `P68 V16 N28` | `GET_MIN` | `ALT_M` | `LANDED` |
| `V37 N29 ENTR` | EVA lunar | `P00 V16 N29` | `GET_MIN` | `EVA_MIN` | `SAMPLE_KG` |
| `V37 N30 ENTR` | subida do LM | `P12 V16 N30` | `GET_MIN` | `BURN_SEC` | `VEL_MS` |
| `V37 N31 ENTR` | rendezvous e docking | `P20 V16 N31` | `GET_MIN` | `RANGE_KM` | `DOCKED` |
| `V37 N32 ENTR` | injecao transterrestre | `P40 V16 N32` | `GET_MIN` | `BURN_SEC` | `DV_MPS` |
| `V37 N33 ENTR` | costa transterrestre | `P23 V16 N33` | `GET_MIN` | `DIST_KKM` | `MCC` |
| `V37 N34 ENTR` | interface de reentrada | `P61 V16 N34` | `GET_MIN` | `ALT_KM` | `VEL_MS` |
| `V37 N35 ENTR` | splashdown e recuperacao | `P67 V16 N35` | `GET_MIN` | `ALT_KM` | `RECOVERY` |
| `V37 N40 ENTR` | aborto de lancamento Mode I | `P70 V16 N40` | `GET_MIN` | `MODE` | `STATUS` |
| `V37 N41 ENTR` | aborto em orbita terrestre | `P37 V16 N41` | `GET_MIN` | `DV_MPS` | `STATUS` |
| `V37 N42 ENTR` | aborto/free-return translunar | `P37 V16 N42` | `GET_MIN` | `MCC` | `STATUS` |
| `V37 N43 ENTR` | aborto da descida lunar | `P71 V16 N43` | `GET_MIN` | `ALT_KM` | `STATUS` |
| `V37 N44 ENTR` | demonstracao alarme `1202` | `P63 V16 N44` | `ALARM` | `RECYCLE` | `STATUS` |
| `V37 N45 ENTR` | demonstracao alarme `1201` | `P63 V16 N45` | `ALARM` | `RECYCLE` | `STATUS` |
| `V37 N46 ENTR` | perda de comunicacao | `P00 V16 N46` | `GET_MIN` | `UPLINK` | `STATUS` |
| `V37 N47 ENTR` | realinhamento IMU | `P52 V16 N47` | `GET_MIN` | `STAR` | `STATUS` |
| `V37 N48 ENTR` | atitude manual / RHC | `P00 V16 N48` | `GET_MIN` | `RHC` | `STATUS` |

## Saida serial esperada

Selecionando uma situacao estatica:

```text
APOLLO11 N28 P68 V16 LANDED SURFACE | GET_MIN 6155 | ALT_M 0 | LANDED 1 | ALM 0000
```

A saida limpa de status tambem inclui o bloco `MSN`:

```text
AGC P68 V16 N28 | R1 +06155 R2 +00000 R3 +00001 | ALM 0000 | JOY +0000,+0000,0 | Z 4000 A 21521 | CYC 123456 | RUN | MSN N28 LANDED SURFACE | GET_MIN 6155 | ALT_M 0 | LANDED 1
```

Selecionando um alarme:

```text
VERB 37 NOUN 44 ENTR
```

Saida esperada:

```text
APOLLO11 N44 P63 V16 PROGRAM ALARM 1202 | ALARM 1202 | RECYCLE 1 | STATUS 0 | ALM 1202
```

## Comandos de teste recomendados

Teste rapido da missao inteira:

```text
V37 N11 ENTR
V37 N20 ENTR
V37 N21 ENTR
V37 N24 ENTR
V37 N26 ENTR
V37 N28 ENTR
V37 N30 ENTR
V37 N32 ENTR
V37 N35 ENTR
```

Teste de situacoes anormais:

```text
V37 N40 ENTR
V37 N43 ENTR
V37 N44 ENTR
V37 N45 ENTR
V37 N46 ENTR
```

Voltar para o modo basico:

```text
V37 N00 ENTR
```

## Limites desta camada

Esta camada e intencionalmente operacional, nao historicamente perfeita:

- nao executa ainda todos os jobs, interrupts e erasable memory do Comanche real
- nao calcula trajetoria orbital real
- nao modela IMU, radar, SPS, DPS, APS, RCS e telemetria em alta fidelidade
- nao substitui os procedimentos completos de checklist da tripulacao

O valor dela agora e permitir testar o DSKY e a arquitetura ESP32/ESP8266 em todos os grandes modos de missao enquanto o core AGC nativo evolui.

## Status dos proximos passos tecnicos

Implementado neste bloco:

- caminho inicial para carregar imagens de rope geradas externamente pelo `yaYUL`
- helper `embedded/tools/build_rope_image.ps1` para validar `Comanche055`/`Luminary099`, executar `yaYUL` quando disponivel e gerar `rope_image.h`
- loader `ROPE,INFO` / `ROPE,LOAD` para o ESP32
- mapa de memoria Block II para erasable direto/chaveado, common fixed e fixed-fixed
- infraestrutura de instrucoes estendidas, `RELINT`, `INHINT`, `RESUME`, interrupts, canais I/O e registradores especiais basicos no core AGC
- decoder Block II explicito para opcodes basicos, extracodes, quarter-code e peripheral-code
- comportamento de leitura/escrita dos registradores de edicao `CYR`, `SR`, `CYL` e `EDOP`
- canais AGC de 9 bits, incluindo aliases `L`/`Q` e bit `SUPERBNK`
- vetores Block II para `T6RUPT`, `T5RUPT`, `T3RUPT`, `T4RUPT`, `KEYRUPT1/2`, `UPRUPT`, `DOWNRUPT`, `RADAR` e `HANDRUPT`
- contagem de ciclos em MCT para aproximar timing de instrucoes, contadores `TIME1..TIME6`, `KEYRUPT` e `DOWNRUPT`
- camada deterministica de perifericos para `KEYRUPT`, `DOWNRUPT`, uplink por teclado e fila de downlink
- manifesto de rope real com hashes quando `yaYUL` gera `rope_image.h`
- mapa inicial de nouns reais do `PINBALL` para consulta serial
- modelo de telemetria de missao com helpers fisicos leves para subida, costa, orbita, descida e reentrada
- extensao `PHASE,...` do protocolo para o Web DSKY exibir fase e nomes dos registradores
- harness `agc_trace_runner` e comparador CSV para validar o core contra traces normalizados do yaAGC/VirtualAGC

Ainda falta para ficar historicamente fiel:

- ter um `yaYUL` executavel no ambiente e validar uma imagem real gerada de Comanche/Luminary no ESP32
- validar a semantica de opcodes, modos de enderecamento, interrupts, downrupt, uplink/downlink e perifericos contra traces do yaAGC/VirtualAGC
- substituir os modelos fisicos leves por simuladores orbitais e de veiculo em alta fidelidade
