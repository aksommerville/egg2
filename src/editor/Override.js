/* This file exists only to be overridden by clients.
 * You can inject dependencies just like any other class.
 */
 
export class Override {
  static getDependencies() {
    return [];
  }
  constructor() {
  
    /* Each action is: {
     *   name: Unique string for internal bookkeeping.
     *   label: String for display in the actions menu.
     *   fn: No-arg function to do the thing.
     * }
     * These will appear in the global actions menu, before standard actions.
     */
    this.actions = [
      //{ name: "doMyThing", label: "Do My Thing", fn: () => this.doMyThing() },
    ];
    
    /* Each editor is a class:
     * class MyEditor {
     *   static checkResource(res) {
     *     // res: { path, type, rid, name, serial: Uint8Array }
     *     // Return 2 if we're the ideal editor (stop searching), 1 if we're an option (continue searching), or 0 if we can't open it.
     *     return 0;
     *   }
     *   setup(res) {
     *     // Called immediately after instantiation.
     *   }
     * }
     * Custom editors will be searched in order, before the standard ones.
     */
    this.editors = [
      //MyEditor,
    ];
  }
};

Override.singleton = true;
