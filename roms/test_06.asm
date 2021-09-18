
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
    lda #<func2
    sta $0200
    lda #>func2
    sta $0201
    lda #$00
    jmp ($0200)

func2:
    inc a
    jmp func2

    .org $FFFA
VECTORS:
    .dw _nmi
    .dw _reset
    .dw _irq
