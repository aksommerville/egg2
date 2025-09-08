# eggdev HTTP Server

Egg provides a trivial HTTP server for running the editor and testing web builds.

We assume a single trusted user. If multiple clients talk to the server at once,
it will work, but they'll get bottlenecked in annoying ways.

## Usage

Launch with `eggdev serve` and arguments:
- `--port=INT`: Default 8080.
- `--unsafe-external`: Serve on all interfaces. Only localhost by default. **See warnings below.**
- `--htdocs=[REMOTE:]LOCAL`: Serve files read-only from `LOCAL`, optionally mapping to request path `REMOTE`.
- - If you omit `REMOTE:`, `LOCAL` is the root. ie `GET /abc/index.html` will serve `LOCAL/abc/index.html`.
- - You may provide more than one. We search the last arguments first. So `--htdocs=my_defaults --htdocs=piecemeal_overrides`.
- - Start the local path with "EGG_SDK/" to serve from the SDK.
- - `LOCAL` may be a Zip file, we'll painstakingly unpack it on every request. Always use a `REMOTE:` prefix in this case, otherwise it matches every request.
- `--writeable=LOCAL`: Mark one local path as accepting PUT and DELETE methods. It must also be listed as `--htdocs`.
- `--project=DIR`: Identify an Egg project, which we use for resource compilation and the `/api/buildfirst/...` endpoint.

`--unsafe-external` will serve on 0.0.0.0, ie any interface.
This usually means that the server is accessible from any host, possibly from the whole internet.
**This server is not suitable for use on the internet.**
We assume a single trusted user.
Malicious or incompetent users could trivially DoS this server, trash your project, and could probably do more serious harm with just a little effort.

The main reason `--unsafe-external` is even an option is for testing on mobile devices locally.
If you're going to use it, please first be sure that there's no untrusted parties on your network,
and that your network is not reachable from beyond your router.
If you don't feel competent to ensure that, **don't use it**.
You can always build the artifacts separately, then serve them with a tool better suited to the task (eg npm http-server).

## Endpoints

Every special request managed by eggdev begins with `/api/`.

### GET /api/webpath

Returns the path to the standalone HTML of this project, relative to project's root.
Typically `/out/MYGAME-web.html`.
Fails if no suitable target is configured.

### GET /api/buildfirst/**

If `--project` was provided, do the equivalent of `eggdev build PROJECT` first.
If that build fails, respond 500 with the build log.
If it succeeds, or no `--project` was provided, proceed with `GET /**`.

### GET /api/symbols

Returns an array of `{nstype,ns,k,v}` for the symbols declared in the project's `shared_symbols.h`.
If no project was specified at launch, or anything else goes wrong, returns an empty array.

### GET /api/instruments

Returns the synth instruments config file as binary EAU (it's stored as EAU-Text).
That should be stored at `EGG_SDK/src/eggdev/instruments.eaut`.
This is read from scratch every time you call it; no need to restart the server if you change the underlying file.
Beware that regular conversion against the default instruments does cache, and you do have to restart to get that.

### GET /api/toc/**

Resolve the remainder of the path and return a JSON array describing all the files under it.
That's an array of strings where each string is the complete path to a regular file (ie something you can GET).

### GET /api/allcontent/**

Similar to `GET /api/toc/**` but read every file under the directory and deliver their content too.
This can be an enormous amount of traffic, so be careful.

Response is binary, zero or more of:
```
 u8 Path length.
... Path.
u24 Content length.
... Content.
```

### POST /api/convert?dstfmt&srcfmt&ns

Equivalent to `eggdev convert`.

`ns` is a symbol namespace for compiling cmdlist types.

Response may contain two headers `X-srcfmt` and `X-dstfmt`. (errors as well as successes).
If the converter produced contextual error messages, we'll put them in the status message.

### PUT /**

If `--writeable` was provided, write the request contents to a new file or overwrite an existing file.

### DELETE /**

If `--writeable` was provided, delete a file.

### GET /**

Search all `--htdocs` directories and serve a static file.
Files are always read from disk for each request, never cached server-side.
Where the same file matches multiple `--htdocs`, the last one wins.

