export function html(...args: any[]): any;
export const templatize: any;
export function dedupingMixin<T>(mixin: T): T;
export function afterNextRender(
  context: any,
  callback: (...args: any[]) => void,
  args?: any[],
): void;
export function beforeNextRender(
  context: any,
  callback: (...args: any[]) => void,
  args?: any[],
): void;
export function calculateSplices(current: any, previous: any): any;

export class PolymerElement extends HTMLElement {}
export type TemplateInstanceBase = any;
export const Polymer: any;
export const microTask: any;

declare global {
  interface HTMLElement {
    register(id: string): void;
  }
}
