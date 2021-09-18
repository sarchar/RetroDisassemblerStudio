
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
    lda #<behind
    sta $10
    lda #>behind
    sta $11
behind:
    jmp ahead
    nop
ahead:
    ldx #$10
    jmp ($0000,x)

    .org $FFFA
VECTORS:
    .dw _nmi
    .dw _reset
    .dw _irq
