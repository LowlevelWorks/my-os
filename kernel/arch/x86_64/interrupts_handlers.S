.intel_syntax noprefix 

.section .text
.code64 


.global invalid_opcode_handler

invalid_opcode_handler:
    cli 
    hlt
