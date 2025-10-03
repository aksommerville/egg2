/* Dom.js
 * Helpers for managing the HTML document.
 * We don't use HTML anywhere, except the wee bootstrap.
 */
 
import { Injector } from "./Injector.js";
import { ModalPickOne, ModalText, ModalError, Toast } from "./modals.js";

export class Dom {
  static getDependencies() {
    return [Window, Document, Injector];
  }
  constructor(window, document, injector) {
    this.window = window;
    this.document = document;
    this.injector = injector;
    
    this.mutationObserver = new window.MutationObserver(events => this.onMutation(events));
    this.mutationObserver.observe(document.body, { subtree: true, childList: true });
  }
  
  /* Create an HTMLElement and append to (parent).
   * (parent) may be null to leave it unattached.
   * (args) may contain:
   *  - Array: CSS class names.
   *  - Object: Attributes or "on-EVENTNAME":function.
   *  - HTMLElement: Append child.
   *  - Anything else: innerText (last wins).
   */
  spawn(parent, tagName, ...args) {
    const element = this.document.createElement(tagName);
    for (const arg of args) {
      if (arg instanceof HTMLElement) {
        element.appendChild(arg);
      } else if (arg instanceof Array) {
        for (const cssClass of arg) element.classList.add(cssClass);
      } else if (arg && (typeof(arg) === "object")) {
        for (const k of Object.keys(arg)) {
          if (k.startsWith("on-")) {
            element.addEventListener(k.substring(3), arg[k]);
          } else {
            element.setAttribute(k, arg[k]);
          }
        }
      } else {
        element.innerText = arg;
      }
    }
    if (parent) parent.appendChild(element);
    return element;
  }
  
  /* Similar to spawn, but takes a controller class instead, and returns an instance of that class.
   * Controllers are expected to declare a dependency of some subclass of HTMLElement, and assign it to "element".
   * If the controller class implements onRemoveFromDom(), we call it as the element gets removed.
   */
  spawnController(parent, controllerClass, overrides) {
    const tagName = this.tagNameForControllerClass(controllerClass);
    const element = this.spawn(parent, tagName, [controllerClass.name]);
    const controller = this.injector.instantiate(controllerClass, [element, ...(overrides || [])]);
    element.__egg_controller = controller;
    return controller;
  }
  
  /* Like spawnController, but it goes in a new modal.
   * We add some logic to dismiss on OOB clicks (why isn't that the browser's default?), and to remove on close.
   */
  spawnModal(controllerClass, overrides) {
    let downInBounds = true;
    const element = this.spawn(this.document.body, "DIALOG", [controllerClass.name], {
      "on-mousedown": event => { // Obnoxious hack to prevent dismissing when one drags from inside the modal to outside, eg selecting text.
        const bounds = element.getBoundingClientRect();
        downInBounds = ((event.x >= bounds.x) && (event.y >= bounds.y) && (event.x < bounds.right) && (event.y < bounds.bottom));
      },
      "on-click": event => {
        const bounds = element.getBoundingClientRect();
        const inBounds = ((event.x >= bounds.x) && (event.y >= bounds.y) && (event.x < bounds.right) && (event.y < bounds.bottom));
        if (!inBounds && !downInBounds) {
          element.close();
          event.preventDefault();
          event.stopPropagation();
        }
      },
      "on-close": event => {
        element.remove();
      },
    });
    const controller = this.injector.instantiate(controllerClass, [element, ...(overrides || [])]);
    element.__egg_controller = controller;
    element.showModal();
    return controller;
  }
  
  /* Resolves with one member of (strings), or null if cancelled.
   */
  modalPickOne(prompt, strings) {
    const modal = this.spawnModal(ModalPickOne);
    modal.setup(prompt, strings);
    return modal.result;
  }
  
  /* Resolves with response, or null if cancelled.
   */
  modalText(prompt, preset) {
    const modal = this.spawnModal(ModalText);
    modal.setup(prompt, preset);
    return modal.result;
  }
  
  modalError(error) {
    const modal = this.spawnModal(ModalError);
    modal.setup(error);
  }
  
  // (color) optional; white if unspecified. Text is black.
  toast(message, color) {
    const toast = this.spawnController(this.document.body, Toast);
    toast.setup(message, color);
  }
  
  /* If you're going to reparent an element (even moving around within the same parent),
   * you must call this first so we don't trigger onRemoveFromDom on it.
   */
  ignoreNextRemoval(element) {
    if (!element) return;
    element.__egg_ignoreNextRemoval = true;
  }
  
  tagNameForControllerClass(clazz) {
    const depClasses = clazz.getDependencies?.() || [];
    for (const depClass of depClasses) {
      if (depClass === HTMLElement) return "DIV";
      if (HTMLElement.isPrototypeOf(depClass)) {
        return this.tagNameForHtmlClass(depClass);
      }
    }
    throw new Error(`Class ${clazz.name} is being used as a DOM controller, but does not have any HTMLElement subclass in its dependencies.`);
  }
  
  tagNameForHtmlClass(clazz) {
    // This is a pain. Browsers don't let you create elements except via `document.createElement` with a tag name.
    // So if we have the element's class instead, we have to determine the tag name ourselves.
    // Is that right? Seems like a pretty obvious miss on W3C's part if so. More likely, I'm the one missing something.
    // It's not that big a deal though: This situation only arises for controllers' principal elements, which are almost always just DIV.
    switch (clazz) {
      case HTMLUListElement: return "UL";
      case HTMLOListElement: return "OL";
      case HTMLListItemElement: return "LI";
      case HTMLAnchorElement: return "A";
      case HTMLDataListElement: "DATA-LIST"; // oddly, my browser uses HTMLElement for these. But the class does exist.
      case HTMLImageElement: return "IMG";
      case HTMLParagraphElement: return "P";
    }
    const match = clazz.name.match(/^HTML(.*)Element$/);
    if (match) return match[1].toUpperCase();
    throw new Error(`Unable to guess tag name for ${clazz.name}`);
  }
  
  onMutation(events) {
    for (const event of events) {
      if (event.type !== "childList") continue;
      if (!event.removedNodes) continue;
      for (const node of event.removedNodes) {
        this.checkRemoval(node);
      }
    }
  }
  
  checkRemoval(node) {
    if (!node) return;
    if (node.__egg_ignoreNextRemoval) {
      delete node.__egg_ignoreNextRemoval;
      return;
    }
    if (node.__egg_controller) {
      const controller = node.__egg_controller;
      delete node.__egg_controller;
      controller.onRemoveFromDom?.();
    }
    if (node.children) {
      for (let i=node.children.length; i-->0; ) {
        this.checkRemoval(node.children[i]);
      }
    }
  }
}

Dom.singleton = true;
