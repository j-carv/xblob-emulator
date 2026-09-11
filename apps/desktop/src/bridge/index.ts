import { BridgeApi } from './types';
import { MockBridge } from './mockBridge';
import { TauriBridge } from './tauriBridge';

let bridgeInstance: BridgeApi | null = null;

export function isTauriEnvironment(): boolean {
  if (typeof window === 'undefined') {
    return false;
  }
  // Check for Tauri v2 runtime internals
  return '__TAURI_INTERNALS__' in window || '__TAURI__' in window;
}

export function createBridge(forceMock = false): BridgeApi {
  const useMockEnv = import.meta.env.VITE_USE_MOCK === 'true';

  if (forceMock || useMockEnv || !isTauriEnvironment()) {
    return new MockBridge();
  }

  return new TauriBridge();
}

export function getBridge(): BridgeApi {
  if (!bridgeInstance) {
    bridgeInstance = createBridge();
  }
  return bridgeInstance;
}

export * from './types';
export { MockBridge } from './mockBridge';
export { TauriBridge } from './tauriBridge';
