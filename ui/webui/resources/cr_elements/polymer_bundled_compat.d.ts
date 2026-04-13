declare module '*polymer/polymer_bundled.min.js' {
  export function html(...args: any[]): any;
  export const templatize: any;
  export function dedupingMixin<T>(mixin: T): T;
  export function afterNextRender(context: any, callback: (...args: any[]) => void, args?: any[]): void;
  export class PolymerElement extends HTMLElement {}
  export type TemplateInstanceBase = any;
}

declare module 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js' {
  export * from '*polymer/polymer_bundled.min.js';
}


declare global {
  interface HTMLElement {
    register(id: string): void;
  }
}

export {};
