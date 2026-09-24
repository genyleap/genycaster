import { loadQuilibriumStorageConfig, QuilibriumSdkStorage } from './quilibrium_sdk.mjs';

export function createStorageAdapter(env = process.env, dependencies = {}) {
  const provider = String(env.GENYLEAP_MEDIA_STORAGE_PROVIDER || 'quilibrium-qstorage').trim().toLowerCase();
  switch (provider) {
    case 'quilibrium-qstorage':
      return new QuilibriumSdkStorage(loadQuilibriumStorageConfig(env), {
        ...dependencies,
        env,
      });
    default:
      throw new Error(`Unsupported media storage provider: ${provider}`);
  }
}
