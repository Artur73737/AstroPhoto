import test from "node:test";
import assert from "node:assert/strict";
import { scrollbarGeometry, pointerProgress } from "../src/lib/scrollbar.ts";

test("thumb reflects visible page size and stays usable on a long page", () => {
  assert.deepEqual(scrollbarGeometry(800, 1000, 4000), { thumbHeight: 200, travel: 600 });
  assert.deepEqual(scrollbarGeometry(800, 1000, 100000), { thumbHeight: 56, travel: 744 });
});
test("drag preserves the grab offset, reaches both ends and clamps outside the rail", () => {
  assert.equal(pointerProgress(390, 60, 30, 600), .5);
  assert.equal(pointerProgress(90, 60, 30, 600), 0);
  assert.equal(pointerProgress(690, 60, 30, 600), 1);
  assert.equal(pointerProgress(-20, 60, 30, 600), 0);
  assert.equal(pointerProgress(900, 60, 30, 600), 1);
});
test("a short page has no draggable travel or division by zero", () => {
  assert.deepEqual(scrollbarGeometry(800, 1000, 700), { thumbHeight: 800, travel: 0 });
  assert.equal(pointerProgress(100, 0, 0, 0), 0);
});
