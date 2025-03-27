/* Comm.js
 */
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
  }
  
  httpBinary(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "arrayBuffer"); }
  httpText(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "text"); }
  httpJson(method, path, query, headers, body) { return this.http(method, path, query, headers, body, "json"); }
  
  http(method, path, query, headers, body, responseType) {
    const options = {
      method,
    };
    if (headers) options.headers = headers;
    if (body) options.body = body;
    return this.window.fetch(this.composeHttpPath(path, query), options).then(rsp => {
      if (!rsp.ok) throw rsp;
      switch (responseType) {
        case "arrayBuffer": return rsp.arrayBuffer();
        case "text": return rsp.text();
        case "json": return rsp.json();
      }
      return rsp;
    });
  }
  
  composeHttpPath(path, query) {
    if (query) {
      let sep = "?";
      for (const k of Object.keys(query)) {
        path += sep;
        sep = "&";
        path += this.window.encodeURIComponent(k);
        path += "=";
        path += this.window.encodeURIComponent(query[k]);
      }
    }
    return path;
  }
}

Comm.singleton = true;
