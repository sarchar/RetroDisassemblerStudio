
START_ADDRESS = $F000
ROM_SIZE = ($10000 - START_ADDRESS) & $FFFF
HEADER_SIZE = 4

    .segment "header", 0, HEADER_SIZE, 0
    .header

    .dw START_ADDRESS
    .dw ROM_SIZE

    .segment "high", START_ADDRESS, ROM_SIZE, HEADER_SIZE
    .high

_nmi:
_reset:
_irq:
    lda #$0A
    sta $10
    lda #$E0
    sta $11
    lda #$00
    ora $0010
    ora $0011
@0:
    jmp @0-

    .org $FFFA
VECTORS:
    .dw _nmi
    .dw _reset
    .dw _irq
