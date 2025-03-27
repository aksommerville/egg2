import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { RootUi } from "./js/RootUi.js";

addEventListener("load", () => {
  const injector = new Injector(window);
  const dom = injector.instantiate(Dom);
  const root = dom.spawnController(dom.document.body, RootUi);
});
