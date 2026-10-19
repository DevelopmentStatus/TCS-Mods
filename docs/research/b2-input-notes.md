# v0.3.8 native input and battle-droid support
Read-only inspection of the currently selected character: directory battledroid, height .42, hierarchy count 14, declared head joint ff, head locator 0a points to joint 05 with offset (-.0000595,-.03544,-.06762). This disproves the assumption that a missing declared head means no actual head for this model. Exception limited to the observed battle-droid directory and existing valid locator/joint checks.

Static PC instruction evidence: 0x6d7978 calls thiscall keyboard poll 0x6d5990 before native input mapping. Poll reads 256-byte DirectInput state and returns its buffer in EAX. DirectInput K scan code 0x25. The mod augments this byte for configured Q in focused gameplay. Native switching runs before HookMove; movement replacement now preserves native switch state rather than overwriting it too late.

Current game process was read only. No injection, input simulation, or live patching performed. New proxy loads on restart.
