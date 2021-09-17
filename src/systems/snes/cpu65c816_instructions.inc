CPU65C816::UC_OPCODE const
    CPU65C816::DECA_UC[]          = { UC_FETCH_A      | UC_DEC | UC_STORE_A     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::EOR_UC[]           = { UC_FETCH_MEMORY | UC_EOR | UC_STORE_A     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::INC_UC[]           = { UC_FETCH_MEMORY | UC_INC | UC_STORE_MEMORY, UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::INCA_UC[]          = { UC_FETCH_A      | UC_INC | UC_STORE_A     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::JMP_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_PC    , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::LDA_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_A     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::LDX_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_X     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::NOP_UC[]           = {                   UC_NOP                  , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::PHA_UC[]           = { UC_FETCH_A      | UC_NOP | UC_STORE_MEMORY, UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::PHD_UC[]           = { UC_FETCH_D      | UC_NOP | UC_STORE_MEMORY, UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::PLA_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_A     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::STA_UC[]           = { UC_FETCH_A      | UC_NOP | UC_STORE_MEMORY, UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::STZ_UC[]           = { UC_FETCH_ZERO   | UC_NOP | UC_STORE_MEMORY, UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const
    CPU65C816::TXS_UC[]           = { UC_FETCH_X      | UC_NOP | UC_STORE_S     , UC_FETCH_OPCODE | UC_STORE_IR };

CPU65C816::UC_OPCODE const 
    CPU65C816::DEAD_INSTRUCTION[] = { UC_DEAD };

CPU65C816::UC_OPCODE const * const CPU65C816::INSTRUCTION_UCs[256] = {
    // 00: BRK        ORA (d,x)         COP               ORA d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 04: TSB d      ORA d             ASL d             ORA [d]
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 08: PHP        ORA #             ASL A             PHD
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, PHD_UC, 
    // 0C: TSB a      ORA a             ASL a             ORA al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 10: BPL r      ORA (d),y         ORA (d)           ORA (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 14: TRB d      ORA d,x           ASL d,x           ORA [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 18: CLC        ORA a,y           INC A             TCS
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, INCA_UC         , DEAD_INSTRUCTION, 
    // 1C: TRB a      ORA a,x           ASL a,x           ORA al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 20: JSR a      AND (d,x)         JSL al            AND d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 24: BIT d      AND d             ROL d             AND [d]
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 28: PLP        AND #             ROL A             PLD
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 2C: BIT a      AND a             ROL a             AND al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 30: BMI r      AND (d),y         AND (d)           AND (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 34: BIT d,x    AND d,x           ROL d,x           AND [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 38: SEC        AND a,y           DEC A             TSC i
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DECA_UC         , DEAD_INSTRUCTION, 
    // 3C: BIT a,x    AND a,x           ROL a,x           AND al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 40: RTI        EOR (d,x)         WDM               EOR d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 44: NVM xyc    EOR d             LSR d             EOR [d]
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 48: PHA        EOR #             LSR A             PHK s
    PHA_UC          , EOR_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 4C: JMP a      EOR a             LSR a             EOR al
    JMP_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 50: BVC r      EOR (d),y         EOR (d)           EOR (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 54: MVN xyc    EOR d,x           LSR d,x           EOR [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 58: CLI        EOR a,y           PHY s             TCD
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 5C: JMP al     EOR a,x           LSR a,x           EOR al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 60: RTS        ADC (d,x)         PER               ADC d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 64: STZ d      ADC d             ROR d             ADC [d]
    STZ_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 68: PLA        ADC #             ROR A             RTL
    PLA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 6C: JMP (a)    ADC a             ROR a             ADC al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 70: BVS r      ADC (d),y         ADC (d)           ADC (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 74: STZ d,x    ADC d,x           ROR d,x           ADC [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 78: SEI        ADC a,y           PLY               TDC
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 7C: JMP (a,x)  ADC a,x           ROR ax            ADC al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 80: BRA r      STA (d,x)         BRL rl            STA d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 84: STY d      STA d             STX d             STA [d]
    DEAD_INSTRUCTION, STA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 88: DEY        BIT #             TXA               PHB
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 8C: STY a      STA a             STX a             STA al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 90: BCC r      STA (d),y         STA (d)           STA (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 94: STY d,x    STA d,x           STX d,y           STA [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // 98: TYA        STA a,y           TXS               TXY
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, TXS_UC          , DEAD_INSTRUCTION, 
    // 9C: STZ a      STA a,x           STZ a,x           STA al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // A0: LDY #      LDA (d,x)         LDX #             LDA d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, LDX_UC          , DEAD_INSTRUCTION, 
    // A4: LDY d      LDA d             LDX d             LDA [d]
    DEAD_INSTRUCTION, LDA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // A8: TAY        LDA #             TAX               PLD
    DEAD_INSTRUCTION, LDA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // AC: LDY a      LDA a             LDX a             LDA al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // B0: BCS r      LDA (d),y         LDA (d)           LDY (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // B4: LDA d,x    LDY d,x           LDX d,y           LDA [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // B8: CLV        LDA a,y           TSX               TYX
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // BC: LDY a,x    LDA a,x           LDX a,y           LDA al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // C0: CPY #      CMP (d,x)         REP #             CMP d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // C4: CPY d      CMP d             DEC d             CMP [d]
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // C8: INY        CMP #             DEX               WAI
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // CC: CPY a      CMP a             DEC a             CMP al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // D0: BNE r      CMP (d),y         CMP (d)           CMP (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // D4: PEI        CMP d,x           DEC d,x           CMP [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // D8: CLD        CMP a,y           PHX               STP
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // DC: JML (a)    CMP a,x           DEC a,x           CMP al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // E0: CPX #      SBC (d,x)         SEP #             SBC d,s
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // E4: CPX d      SBC d             INC d             SBC [d]
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, INC_UC          , DEAD_INSTRUCTION, 
    // E8: INX        SBC #             NOP               XBA
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, NOP_UC          , DEAD_INSTRUCTION, 
    // EC: CPX a      SBC a             INC a             SBC al
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // F0: BEQ r      SBC (d),y         SBC (d)           SBC (d,s),y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // F4: PEA        SBC d,x           INC d,x           SBC [d],y
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, INC_UC          , DEAD_INSTRUCTION, 
    // F8: SED        SBC a,y           PLX               XCE
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    // FC: JSR (a,x)  SBC a,x           INC a,x           SBC al,x
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
};

CPU65C816::ADDRESSING_MODE const CPU65C816::INSTRUCTION_ADDRESSING_MODES[256] = {
    // 00: BRK         ORA (d,x)          COP                  ORA d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 04: TSB d       ORA d              ASL d                ORA [d]
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 08: PHP         ORA #              ASL A                PHD
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_STACK  ,
    // 0C: TSB a       ORA a              ASL a                ORA al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 10: BPL r       ORA (d),y          ORA (d)              ORA (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 14: TRB d       ORA d,x            ASL d,x              ORA [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 18: CLC         ORA a,y            INC A                TCS
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 1C: TRB a       ORA a,x            ASL a,x              ORA al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 20: JSR a       AND (d,x)          JSL al               AND d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 24: BIT d       AND d              ROL d                AND [d]
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 28: PLP         AND #              ROL A                PLD
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 2C: BIT a       AND a              ROL a                AND al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 30: BMI r       AND (d),y          AND (d)              AND (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 34: BIT d,x     AND d,x            ROL d,x              AND [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 38: SEC         AND a,y            DEC A                TSC i
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 3C: BIT a,x     AND a,x            ROL a,x              AND al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 40: RTI         EOR (d,x)          WDM                  EOR d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 44: NVM xyc     EOR d              LSR d                EOR [d]
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 48: PHA         EOR #              LSR A                PHK s
    AM_STACK         , AM_IMMEDIATE_BYTE, AM_IMPLIED         , AM_IMPLIED,
    // 4C: JMP a       EOR a              LSR a                EOR al
    AM_IMMEDIATE_WORD, AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 50: BVC r       EOR (d),y          EOR (d)              EOR (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 54: MVN xyc     EOR d,x            LSR d,x              EOR [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 58: CLI         EOR a,y            PHY s                TCD
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 5C: JMP al      EOR a,x            LSR a,x              EOR al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 60: RTS         ADC (d,x)          PER                  ADC d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 64: STZ d       ADC d              ROR d                ADC [d]
    AM_DIRECT_PAGE   , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 68: PLA         ADC #              ROR A                RTL
    AM_STACK         , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 6C: JMP (a)     ADC a              ROR a                ADC al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 70: BVS r       ADC (d),y          ADC (d)              ADC (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 74: STZ d,x     ADC d,x            ROR d,x              ADC [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 78: SEI         ADC a,y            PLY                  TDC
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 7C: JMP (a,x)   ADC a,x            ROR ax               ADC al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 80: BRA r       STA (d,x)          BRL rl               STA d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 84: STY d       STA d              STX d                STA [d]
    AM_IMPLIED       , AM_DIRECT_PAGE   , AM_IMPLIED         , AM_IMPLIED,
    // 88: DEY         BIT #              TXA                  PHB
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 8C: STY a       STA a              STX a                STA al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 90: BCC r       STA (d),y          STA (d)              STA (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 94: STY d,x     STA d,x            STX d,y              STA [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 98: TYA         STA a,y            TXS                  TXY
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // 9C: STZ a       STA a,x            STZ a,x              STA al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // A0: LDY #       LDA (d,x)          LDX #                LDA d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMMEDIATE_BYTE  , AM_IMPLIED,
    // A4: LDY d       LDA d              LDX d                LDA [d]
    AM_IMPLIED       , AM_DIRECT_PAGE   , AM_IMPLIED         , AM_IMPLIED,
    // A8: TAY         LDA #              TAX                  PLD
    AM_IMPLIED       , AM_IMMEDIATE_BYTE, AM_IMPLIED         , AM_IMPLIED,
    // AC: LDY a       LDA a              LDX a                LDA al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // B0: BCS r       LDA (d),y          LDA (d)              LDY (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // B4: LDA d,x     LDY d,x            LDX d,y              LDA [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // B8: CLV         LDA a,y            TSX                  TYX
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // BC: LDY a,x     LDA a,x            LDX a,y              LDA al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // C0: CPY #       CMP (d,x)          REP #                CMP d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // C4: CPY d       CMP d              DEC d                CMP [d]
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // C8: INY         CMP #              DEX                  WAI
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // CC: CPY a       CMP a              DEC a                CMP al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // D0: BNE r       CMP (d),y          CMP (d)              CMP (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // D4: PEI         CMP d,x            DEC d,x              CMP [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // D8: CLD         CMP a,y            PHX                  STP
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // DC: JML (a)     CMP a,x            DEC a,x              CMP al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // E0: CPX #       SBC (d,x)          SEP #                SBC d,s
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // E4: CPX d       SBC d              INC d                SBC [d]
    AM_IMPLIED       , AM_IMPLIED       , AM_DIRECT_PAGE     , AM_IMPLIED,
    // E8: INX         SBC #              NOP                  XBA
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // EC: CPX a       SBC a              INC a                SBC al
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // F0: BEQ r       SBC (d),y          SBC (d)              SBC (d,s),y
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // F4: PEA         SBC d,x            INC d,x              SBC [d],y
    AM_IMPLIED       , AM_IMPLIED       , AM_DIRECT_INDEXED_X, AM_IMPLIED,
    // F8: SED         SBC a,y            PLX                  XCE
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
    // FC: JSR (a,x)   SBC a,x            INC a,x              SBC al,x
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED         , AM_IMPLIED,
};

