/* Injector.js
 * Generic dependency injector.
 * Injectable classes should implement `static getDependencies()` returning an array of classes
 * corresponding to the constructor's arguments.
 * A dependency "nonce" will receive some positive integer that has not been used before.
 * If the class has a true member "singleton", it will only be instantiated once, then shared around.
 * Overrides to singleton instantiation are only respected the first time it gets asked for.
 */
 
export class Injector {

  /* getDependencies() will never be called for us, just setting a good example.
   */
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    if (!Injector.globalSingleton) {
      Injector.globalSingleton = this;
    }
    
    this.singletons = {
      Injector: this,
      Window: window,
      Document: window.document,
      Navigator: window.navigator || {},
    };
    this.inProgress = [];
    this.nextNonce = 1;
  }
  
  /* Use this to acquire the injector (and thru it, anything) from any static context.
   * Undefined if Injector has never been instantiated.
   * Make an effort not to need this!
   */
  static getGlobalSingleton() {
    return Injector.globalSingleton;
  }
  
  instantiate(clazz, overrides) {
    if (clazz === "nonce") return this.nextNonce++;
    const name = clazz.name;
    let instance = this.singletons[name];
    if (instance) return instance;
    if (this.inProgress.includes(name)) {
      throw new Error(`Dependency cycle involving these classes: ${this.inProgress.join(', ')}`);
    }
    this.inProgress.push(name);
    try {
      instance = this.instantiateInner(clazz, overrides || []);
    } finally {
      const p = this.inProgress.indexOf(name);
      if (p >= 0) this.inProgress.splice(p, 1);
    }
    if (clazz.singleton) {
      this.singletons[name] = instance;
    }
    return instance;
  }
  
  // Not responsible for (inProgress) or (singletons).
  instantiateInner(clazz, overrides) {
    if (clazz instanceof HTMLElement) {
      throw new Error(`Injector was asked to instantiate ${clazz.name}. Did you forget an override?`);
    }
    const depClasses = clazz.getDependencies?.() || [];
    const deps = [];
    for (const depClass of depClasses) {
      let dep = (typeof(depClass) === "function") ? overrides.find(o => o instanceof depClass) : null;
      if (dep) deps.push(dep);
      else deps.push(this.instantiate(depClass, overrides));
    }
    return new clazz(...deps);
  }
}

Injector.singleton = true;
