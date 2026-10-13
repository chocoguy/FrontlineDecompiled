// Calls through a pointer to member function held in r12 (this in r3).
asm void __ptmf_scall(void) {
    nofralloc
    lwz r0, 0(r12)
    lwz r11, 4(r12)
    lwz r12, 8(r12)
    add r3, r3, r0
    cmpwi r11, 0
    blt _nonvirtual
    lwzx r12, r3, r12
    lwzx r12, r12, r11
_nonvirtual:
    mtctr r12
    bctr
}
