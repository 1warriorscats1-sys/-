/* thWWW Switch mapping. Original integration: MIT.
 *
 * Wonderful Waking World is a keyboard-first vertical danmaku game: shoot,
 * bomb and slow-move are the only inputs that matter, so the pad layout is
 * kept in the Touhou convention:
 *   A -> shoot (GML button 0), B -> bomb (GML button 1),
 *   ZL/ZR/L/R -> focus (slow) movement, D-pad and left stick -> movement.
 * The right stick is inert, including libnx's synthesized directional bits
 * and its click, so a resting stick can never nudge the player hitbox. */
static void mapLibnxToGml(GamepadSlot* slot, PadState* pad, u64 cur) {
    cur &= ~(HidNpadButton_StickR | HidNpadButton_StickRLeft |
             HidNpadButton_StickRUp | HidNpadButton_StickRRight |
             HidNpadButton_StickRDown);
    if (cur & HidNpadButton_A) slot->buttonDown[0] = true;
    if (cur & HidNpadButton_B) slot->buttonDown[1] = true;
    if (cur & HidNpadButton_Y) slot->buttonDown[2] = true;
    if (cur & HidNpadButton_X) slot->buttonDown[3] = true;
    if (cur & HidNpadButton_L) slot->buttonDown[4] = true;
    if (cur & HidNpadButton_R) slot->buttonDown[5] = true;
    slot->buttonValue[6] = (cur & HidNpadButton_ZL) ? 1.0f : 0.0f;
    slot->buttonValue[7] = (cur & HidNpadButton_ZR) ? 1.0f : 0.0f;
    if (cur & HidNpadButton_Minus) slot->buttonDown[8] = true;
    if (cur & HidNpadButton_Plus) slot->buttonDown[9] = true;
    if (cur & HidNpadButton_StickL) slot->buttonDown[10] = true;
    slot->buttonDown[11] = false;
    if (cur & HidNpadButton_AnyUp) slot->buttonDown[12] = true;
    if (cur & HidNpadButton_AnyDown) slot->buttonDown[13] = true;
    if (cur & HidNpadButton_AnyLeft) slot->buttonDown[14] = true;
    if (cur & HidNpadButton_AnyRight) slot->buttonDown[15] = true;

    HidAnalogStickState l = padGetStickPos(pad, 0);
    slot->axisValue[0] = l.x / 32767.0f;
    slot->axisValue[1] = -l.y / 32767.0f;
    slot->axisValue[2] = 0.0f;
    slot->axisValue[3] = 0.0f;
}
