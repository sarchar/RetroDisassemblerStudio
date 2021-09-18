
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
    ; put address $0210 in $02
    lda #$10
    sta $02
    lda #$02
    sta $03
    ; set offset to $02
    ldx #$02
    ; set value to store
    lda #$42
    sta ($00,x)
    ; memory $0210 should contain $42
    ; zero out $0210
    lda #$00
    sta ($02)
    ; overwrite $02 with $00
    stz $02
    ; put $10 into Y
    ldy #$10
    ; write $E8
    lda #$E8
    sta ($02),y
    ; memory at $210 should contain $E8
@0:
    jmp @0-

    .org $FFFA
VECTORS:
    .dw _nmi
    .dw _reset
    .dw _irq
