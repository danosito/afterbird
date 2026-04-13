declare module '//resources/polymer/v3_0/polymer/polymer_bundled.min.js' {
  export const html: any;
  export const templatize: any;
  export const dedupingMixin: any;
  export type TemplateInstanceBase = any;
  export class PolymerElement extends HTMLElement {}
}

declare module 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js' {
  export * from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
}
