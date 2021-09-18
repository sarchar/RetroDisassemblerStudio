
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
    ; test EOR a,x
    lda #$FF
    sta $0210
    lda #$A5
    sta $0211
    lda #$C3
    ldx #$10
    eor $0200,x
    ; A should contain $3C
    inx
    eor $0200,x
    ; A should contain $99
@0:
    jmp @0-

    .org $FFFA
VECTORS:
    .dw _nmi
    .dw _reset
    .dw _irq
