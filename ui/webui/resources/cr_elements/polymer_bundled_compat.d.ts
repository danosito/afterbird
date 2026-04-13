export function html(...args: any[]): any;
export const templatize: any;

export type SpliceRecord = {
  index: number;
  removed: any[];
  addedCount: number;
};

export function calculateSplices(
  current: any[],
  previous: any[],
): SpliceRecord[];

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

export class PolymerElement extends HTMLElement {
  root: Document | ShadowRoot | Element;
  connectedCallback(): void;
  disconnectedCallback(): void;
  ready(): void;
  get(path: string, root?: any): any;
  set(path: string, value: any): void;
  notifySplices(path: string, splices: SpliceRecord[]): void;
}

export type TemplateInstanceBase = any;
export const Polymer: any;
export const microTask: any;

declare global {
  interface HTMLElement {
    register(id: string): void;
  }
}
