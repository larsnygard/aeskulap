// Cornerstone3D glue for the Aeskulap web viewer.
// Modern replacement for the 2005 gtkmm/glademm rendering widgets.
import {
  init as coreInit,
  RenderingEngine,
  eventTarget,
  setUseCPURendering,
  Enums,
  utilities,
} from '@cornerstonejs/core';
import {
  init as toolsInit,
  addTool,
  ToolGroupManager,
  PanTool,
  WindowLevelTool,
  ZoomTool,
  StackScrollTool,
  LengthTool,
  ProbeTool,
  MagnifyTool,
  Enums as ToolsEnums,
} from '@cornerstonejs/tools';
import dicomImageLoader from '@cornerstonejs/dicom-image-loader';

const { ViewportType, Events } = Enums;
const { MouseBindings } = ToolsEnums;

export type ToolName =
  | 'Pan'
  | 'WindowLevel'
  | 'Zoom'
  | 'StackScroll'
  | 'Length'
  | 'Probe'
  | 'Magnify';

let initialized = false;

export async function initCornerstone(): Promise<void> {
  if (initialized) return;
  await coreInit({ rendering: { useCPURendering: false }, debug: {} } as any);
  dicomImageLoader.init({ maxWebWorkers: Math.max(1, (navigator.hardwareConcurrency || 4) >> 1) });
  toolsInit();
  addTool(PanTool);
  addTool(WindowLevelTool);
  addTool(ZoomTool);
  addTool(StackScrollTool);
  addTool(LengthTool);
  addTool(ProbeTool);
  addTool(MagnifyTool);
  initialized = true;
}

export const TOOL_MAP: Record<ToolName, string> = {
  Pan: PanTool.toolName,
  WindowLevel: WindowLevelTool.toolName,
  Zoom: ZoomTool.toolName,
  StackScroll: StackScrollTool.toolName,
  Length: LengthTool.toolName,
  Probe: ProbeTool.toolName,
  Magnify: MagnifyTool.toolName,
};

export class Viewer {
  renderingEngine: RenderingEngine;
  toolGroup!: NonNullable<ReturnType<typeof ToolGroupManager.createToolGroup>>;
  viewportIds: string[] = [];
  activeId: string;

  constructor() {
    this.renderingEngine = new RenderingEngine('aeskulap');
    this.toolGroup = ToolGroupManager.createToolGroup('main')!;
    for (const name of Object.keys(TOOL_MAP)) {
      this.toolGroup.addTool(TOOL_MAP[name as ToolName]);
    }
    this.toolGroup.setToolActive(TOOL_MAP.WindowLevel, {
      bindings: [{ mouseButton: MouseBindings.Primary }],
    });
    this.toolGroup.setToolActive(TOOL_MAP.Pan, {
      bindings: [{ mouseButton: MouseBindings.Secondary }],
    });
    this.toolGroup.setToolActive(TOOL_MAP.Zoom, {
      bindings: [{ mouseButton: MouseBindings.Wheel }],
    });
    this.toolGroup.setToolActive(TOOL_MAP.StackScroll, {
      bindings: [{ mouseButton: MouseBindings.Wheel_Primary }],
    });
    this.activeId = '';
  }

  setGrid(cols: number, rows: number, elements: HTMLDivElement[]) {
    this.disableAll();
    const inputs = elements.slice(0, cols * rows).map((element, i) => ({
      viewportId: `vp-${i}`,
      element,
      type: ViewportType.STACK,
    }));
    this.viewportIds = inputs.map((i) => i.viewportId);
    for (const input of inputs) this.renderingEngine.enableElement(input);
    for (const id of this.viewportIds) this.toolGroup.addViewport(id, this.renderingEngine.id);
    this.activeId = this.viewportIds[0];
  }

  disableAll() {
    for (const id of this.viewportIds) {
      try {
        this.renderingEngine.disableElement(id);
      } catch {
        /* already disabled */
      }
    }
    this.viewportIds = [];
  }

  setActive(id: string) {
    this.activeId = id;
  }

  async loadSeries(id: string, urls: string[], startIndex = 0) {
    const imageIds = urls.map((u) => `dicomweb:${u}`);
    const viewport = this.renderingEngine.getViewport(id) as any;
    await viewport.setStack(imageIds, startIndex);
    viewport.render();
    try {
      viewport.setCamera?.({ flipHorizontal: true });
    } catch {
      /* not supported on this render path */
    }
    return imageIds;
  }

  setTool(name: ToolName) {
    this.toolGroup.setActivePrimaryTool(TOOL_MAP[name]);
  }

  get activeViewport() {
    return this.renderingEngine.getViewport(this.activeId) as any;
  }

  scroll(delta: number) {
    this.activeViewport?.scroll?.(delta, false);
  }

  setInvert(on: boolean) {
    this.activeViewport?.setProperties?.({ invert: on });
  }

  reset() {
    for (const id of this.viewportIds) {
      const vp = this.renderingEngine.getViewport(id) as any;
      vp?.resetCamera?.();
      vp?.setProperties?.({ invert: false });
      vp?.resetProperties?.();
      vp?.render?.();
    }
  }

  onNewImage(cb: (id: string, index: number, count: number) => void) {
    // STACK_NEW_IMAGE is dispatched on each viewport's DOM element.
    const attach = () => {
      const cleanups: (() => void)[] = [];
      for (const id of this.viewportIds) {
        const el = (this.renderingEngine.getViewport(id) as any)?.element as HTMLElement | undefined;
        if (!el) continue;
        const onEvent = (e: Event) => {
          const d = (e as CustomEvent).detail;
          const read = () => {
            const vp = this.renderingEngine.getViewport(id) as any;
            if (!vp) return;
            const ids = vp.getImageIds?.() ?? [];
            if (ids.length === 0) return; // mid-setStack; the rAF tick reads real state
            cb(id, d?.imageIdIndex ?? vp.getCurrentImageIdIndex(), ids.length);
          };
          read();
          requestAnimationFrame(read);
        };
        el.addEventListener(Events.STACK_NEW_IMAGE, onEvent);
        cleanups.push(() => el.removeEventListener(Events.STACK_NEW_IMAGE, onEvent));
      }
      return cleanups;
    };
    let cleanups = attach();
    const rescan = setInterval(() => {
      if (this.viewportIds.length !== cleanups.length) {
        cleanups.forEach((c) => c());
        cleanups = attach();
      }
    }, 1000);
    return () => {
      clearInterval(rescan);
      cleanups.forEach((c) => c());
    };
  }
}
