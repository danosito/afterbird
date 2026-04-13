declare module '*polymer/polymer_bundled.min.js' {
  export const html: (...args: any[]) => any;
  export const templatize: any;
  export const dedupingMixin: any;
  export type TemplateInstanceBase = any;
  export type PolymerElement = any;
}

declare module '//resources/polymer/v3_0/polymer/polymer_bundled.min.js' {
  export * from '*polymer/polymer_bundled.min.js';
}

declare module 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js' {
  export * from '*polymer/polymer_bundled.min.js';
}
