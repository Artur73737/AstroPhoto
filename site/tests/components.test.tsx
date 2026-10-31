import test, { beforeEach, afterEach } from "node:test";
import assert from "node:assert/strict";
import { Window } from "happy-dom";
import { act, useState } from "react";
import { createRoot, type Root } from "react-dom/client";
import { SideRuler } from "../src/components/SideRuler/SideRuler";
import { ResourcesMenu } from "../src/components/ResourcesMenu/ResourcesMenu";
import { MobileMenu } from "../src/components/MobileMenu/MobileMenu";
import { WorkspaceSection } from "../src/components/WorkspaceSection";
import { RulerDigits } from "../src/components/RulerDigits/RulerDigits";
import { getStoredTheme, storeTheme } from "../src/services/theme/themeService";
import type { NavigationContent } from "../src/types/content";

let window: Window;
let root: Root;
let container: HTMLElement;
let scrolledTo: number[];
let captured: Set<number>;

beforeEach(() => {
  window = new Window({ url: "http://localhost" });
  Object.assign(globalThis, {
    window, document: window.document, HTMLElement: window.HTMLElement,
    Node: window.Node, PointerEvent: window.PointerEvent,
    IS_REACT_ACT_ENVIRONMENT: true,
    ResizeObserver: class { observe() {} disconnect() {} },
    requestAnimationFrame: (callback: FrameRequestCallback) => { callback(0); return 1; },
  });
  Object.defineProperty(window, "innerHeight", { value: 1000, configurable: true });
  Object.defineProperty(window.document.documentElement, "scrollHeight", { value: 5000, configurable: true });
  scrolledTo = [];
  window.scrollTo = ((options: ScrollToOptions) => { scrolledTo.push(options.top!); }) as typeof window.scrollTo;
  captured = new Set();
  window.HTMLElement.prototype.setPointerCapture = function(id: number) { captured.add(id); };
  window.HTMLElement.prototype.hasPointerCapture = function(id: number) { return captured.has(id); };
  window.HTMLElement.prototype.releasePointerCapture = function(id: number) { captured.delete(id); };
  const element = window.document.createElement("div");
  window.document.body.append(element);
  container = element as unknown as HTMLElement;
  root = createRoot(container);
});
afterEach(async () => { await act(async () => root.unmount()); await window.happyDOM.abort(); });

async function mountScrollbar(progress = 0) {
  await act(async () => root.render(<SideRuler label="Page position" progress={progress} scrollY={0} documentHeight={5000} viewportHeight={1000} />));
  const track = container.querySelector<HTMLElement>('[role="scrollbar"]')!;
  const thumb = container.querySelector<HTMLElement>(".ruler-thumb")!;
  track.getBoundingClientRect = () => ({ top: 100, height: 800 } as DOMRect);
  thumb.getBoundingClientRect = () => ({ top: 100, height: 160 } as DOMRect);
  return { track, thumb };
}
function pointer(type: string, y: number, id = 7) {
  return new window.PointerEvent(type, { pointerId: id, isPrimary: true, button: 0, clientY: y, bubbles: true });
}
test("dragging a grabbed thumb preserves position, follows the pointer and releases capture", async () => {
  const { track, thumb } = await mountScrollbar();
  await act(async () => { thumb.dispatchEvent(pointer("pointerdown", 130) as unknown as Event); });
  assert.equal(captured.has(7), true);
  assert.equal(scrolledTo.at(-1), 0);
  await act(async () => { track.dispatchEvent(pointer("pointermove", 450) as unknown as Event); });
  assert.equal(scrolledTo.at(-1), 2000);
  await act(async () => { track.dispatchEvent(pointer("pointermove", 1000) as unknown as Event); });
  assert.equal(scrolledTo.at(-1), 4000);
  await act(async () => { track.dispatchEvent(pointer("pointerup", 1000) as unknown as Event); });
  assert.equal(captured.size, 0);
  const count = scrolledTo.length;
  await act(async () => { track.dispatchEvent(pointer("pointermove", 200) as unknown as Event); });
  assert.equal(scrolledTo.length, count);
});
test("clicking the track positions the thumb and keyboard navigation reaches both ends", async () => {
  const { track } = await mountScrollbar(.5);
  await act(async () => track.dispatchEvent(pointer("pointerdown", 420) as unknown as Event));
  assert.equal(scrolledTo.at(-1), 1500);
  await act(async () => track.dispatchEvent(pointer("pointercancel", 420) as unknown as Event));
  for (const [key, expected] of [["End", 4000], ["Home", 0], ["PageDown", 3000], ["ArrowUp", 1920]] as const) {
    await act(async () => track.dispatchEvent(new window.KeyboardEvent("keydown", { key, bubbles: true }) as unknown as Event));
    assert.equal(scrolledTo.at(-1), expected);
  }
});
test("the percentage is a single legible value and scale labels stay in 0–100", async () => {
  await mountScrollbar(.57);
  assert.equal(container.querySelector(".ruler-digits")?.textContent, "57");
  assert.deepEqual([...container.querySelectorAll(".ruler-tick > span")].map(node => node.textContent), ["00", "25", "50", "75", "100"]);
  assert.equal(container.querySelector('[role="scrollbar"]')?.getAttribute("aria-valuenow"), "57");
  await act(async () => root.render(<RulerDigits value={100} />));
  assert.equal(container.textContent, "100");
});
test("documentation closes on link selection and Escape restores the toggle focus", async () => {
  function Menu() {
    const [open, setOpen] = useState(false);
    return <ul><ResourcesMenu open={open} onToggle={() => setOpen(value => !value)} menu={{label:"Documentation",items:[{label:"Guide",href:"#guide"}]}} /></ul>;
  }
  await act(async () => root.render(<Menu />));
  const toggle = container.querySelector("button")!;
  await act(async () => toggle.click());
  assert.equal(toggle.getAttribute("aria-expanded"), "true");
  await act(async () => container.querySelector("a")!.click());
  assert.equal(toggle.getAttribute("aria-expanded"), "false");
  await act(async () => toggle.click());
  await act(async () => toggle.dispatchEvent(new window.KeyboardEvent("keydown", {key:"Escape",bubbles:true}) as unknown as Event));
  assert.equal(toggle.getAttribute("aria-expanded"), "false");
  assert.equal(window.document.activeElement, toggle);
});
test("mobile menu uses its open state, closes with Escape and restores focus", async () => {
  const content = { main:[{label:"Workspace",href:"#workspace"}], resourcesMenu:{label:"Documentation",items:[{label:"Guide",href:"#guide"}]},featured:{label:"GitHub",href:"https://github.com/Artur73737/AstroPhoto"} } as NavigationContent;
  function Menu() {
    const [open, setOpen] = useState(false);
    return <MobileMenu open={open} content={content} onToggle={() => setOpen(value=>!value)} onClose={() => setOpen(false)} openLabel="Open main menu" closeLabel="Close main menu" />;
  }
  await act(async () => root.render(<Menu />));
  const toggle = container.querySelector("button")!;
  await act(async () => toggle.click());
  assert.equal(toggle.getAttribute("aria-label"), "Close main menu");
  assert.ok(container.querySelector("#mobile-nav"));
  await act(async () => toggle.dispatchEvent(new window.KeyboardEvent("keydown", {key:"Escape",bubbles:true}) as unknown as Event));
  assert.equal(container.querySelector("#mobile-nav"), null);
  assert.equal(window.document.activeElement, toggle);
});
test("blocked browser storage does not break the theme control", () => {
  Object.defineProperty(window, "localStorage", {get() {throw new Error("Storage blocked");}, configurable:true});
  assert.equal(getStoredTheme(), null);
  assert.doesNotThrow(() => storeTheme("dark"));
});

test("screenshot tabs switch image, accessible selection and keyboard focus together", async () => {
  await act(async () => root.render(<WorkspaceSection />));
  const tabs = container.querySelectorAll<HTMLButtonElement>('[role="tab"]');
  assert.equal(tabs[0].getAttribute("aria-selected"), "true");
  await act(async () => tabs[0].dispatchEvent(new window.KeyboardEvent("keydown", {key:"ArrowRight",bubbles:true}) as unknown as Event));
  assert.equal(tabs[1].getAttribute("aria-selected"), "true");
  assert.equal(tabs[0].tabIndex, -1);
  assert.equal(window.document.activeElement, tabs[1]);
  assert.ok(container.querySelector("img")?.getAttribute("src")?.endsWith("/screenshots/camera.png"));
  assert.equal(container.querySelector('[role="tabpanel"]')?.getAttribute("aria-labelledby"), "tab-camera");
  await act(async () => tabs[1].dispatchEvent(new window.KeyboardEvent("keydown", {key:"End",bubbles:true}) as unknown as Event));
  assert.equal(tabs[2].getAttribute("aria-selected"), "true");
  assert.ok(container.querySelector("img")?.getAttribute("src")?.endsWith("/screenshots/sequences.png"));
});
