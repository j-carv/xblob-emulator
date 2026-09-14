import { HostInputSnapshot } from '../../bridge/types';

export const DIGITAL_BUTTONS = {
  DPAD_UP: 1 << 0,
  DPAD_DOWN: 1 << 1,
  DPAD_LEFT: 1 << 2,
  DPAD_RIGHT: 1 << 3,
  START: 1 << 4,
  BACK: 1 << 5,
  LEFT_THUMB: 1 << 6,
  RIGHT_THUMB: 1 << 7,
} as const;

export function buildHostInputSnapshot(
  sequence: number,
  pressedKeys: Set<string>,
  gamepad: Gamepad | null
): HostInputSnapshot {
  let digital = 0;
  let a = 0;
  let b = 0;
  let x = 0;
  let y = 0;
  let black = 0;
  let white = 0;
  let triggerL = 0;
  let triggerR = 0;
  let thumbLx = 0;
  let thumbLy = 0;
  let thumbRx = 0;
  let thumbRy = 0;

  // Keyboard mapping
  if (pressedKeys.has('ArrowUp') || pressedKeys.has('KeyW')) digital |= DIGITAL_BUTTONS.DPAD_UP;
  if (pressedKeys.has('ArrowDown') || pressedKeys.has('KeyS')) digital |= DIGITAL_BUTTONS.DPAD_DOWN;
  if (pressedKeys.has('ArrowLeft') || pressedKeys.has('KeyA')) digital |= DIGITAL_BUTTONS.DPAD_LEFT;
  if (pressedKeys.has('ArrowRight') || pressedKeys.has('KeyD')) digital |= DIGITAL_BUTTONS.DPAD_RIGHT;
  if (pressedKeys.has('Enter') || pressedKeys.has('KeyP')) digital |= DIGITAL_BUTTONS.START;
  if (pressedKeys.has('Backspace') || pressedKeys.has('KeyO')) digital |= DIGITAL_BUTTONS.BACK;

  if (pressedKeys.has('Space') || pressedKeys.has('KeyJ')) a = 255;
  if (pressedKeys.has('Escape') || pressedKeys.has('KeyK')) b = 255;
  if (pressedKeys.has('KeyU')) x = 255;
  if (pressedKeys.has('KeyI')) y = 255;
  if (pressedKeys.has('KeyE')) black = 255;
  if (pressedKeys.has('KeyQ')) white = 255;
  if (pressedKeys.has('ShiftLeft') || pressedKeys.has('ShiftRight')) triggerL = 255;
  if (pressedKeys.has('ControlLeft') || pressedKeys.has('ControlRight')) triggerR = 255;

  // Gamepad mapping (standard Xbox gamepad layout if connected)
  if (gamepad && gamepad.connected) {
    const btns = gamepad.buttons;
    if (btns[12]?.pressed) digital |= DIGITAL_BUTTONS.DPAD_UP;
    if (btns[13]?.pressed) digital |= DIGITAL_BUTTONS.DPAD_DOWN;
    if (btns[14]?.pressed) digital |= DIGITAL_BUTTONS.DPAD_LEFT;
    if (btns[15]?.pressed) digital |= DIGITAL_BUTTONS.DPAD_RIGHT;
    if (btns[9]?.pressed) digital |= DIGITAL_BUTTONS.START;
    if (btns[8]?.pressed) digital |= DIGITAL_BUTTONS.BACK;
    if (btns[10]?.pressed) digital |= DIGITAL_BUTTONS.LEFT_THUMB;
    if (btns[11]?.pressed) digital |= DIGITAL_BUTTONS.RIGHT_THUMB;

    if (btns[0]?.pressed) a = Math.round((btns[0].value || 1) * 255);
    if (btns[1]?.pressed) b = Math.round((btns[1].value || 1) * 255);
    if (btns[2]?.pressed) x = Math.round((btns[2].value || 1) * 255);
    if (btns[3]?.pressed) y = Math.round((btns[3].value || 1) * 255);
    if (btns[4]?.pressed) white = Math.round((btns[4].value || 1) * 255);
    if (btns[5]?.pressed) black = Math.round((btns[5].value || 1) * 255);
    if (btns[6]) triggerL = Math.round((btns[6].value || 0) * 255);
    if (btns[7]) triggerR = Math.round((btns[7].value || 0) * 255);

    if (gamepad.axes.length >= 2) {
      thumbLx = Math.round((gamepad.axes[0] || 0) * 32767);
      thumbLy = Math.round(-(gamepad.axes[1] || 0) * 32767);
    }
    if (gamepad.axes.length >= 4) {
      thumbRx = Math.round((gamepad.axes[2] || 0) * 32767);
      thumbRy = Math.round(-(gamepad.axes[3] || 0) * 32767);
    }
  }

  return {
    sequence,
    connected: true,
    digitalButtons: digital,
    buttonA: Math.min(255, Math.max(0, a)),
    buttonB: Math.min(255, Math.max(0, b)),
    buttonX: Math.min(255, Math.max(0, x)),
    buttonY: Math.min(255, Math.max(0, y)),
    buttonBlack: Math.min(255, Math.max(0, black)),
    buttonWhite: Math.min(255, Math.max(0, white)),
    triggerLeft: Math.min(255, Math.max(0, triggerL)),
    triggerRight: Math.min(255, Math.max(0, triggerR)),
    thumbLx: Math.min(32767, Math.max(-32768, thumbLx)),
    thumbLy: Math.min(32767, Math.max(-32768, thumbLy)),
    thumbRx: Math.min(32767, Math.max(-32768, thumbRx)),
    thumbRy: Math.min(32767, Math.max(-32768, thumbRy)),
  };
}
