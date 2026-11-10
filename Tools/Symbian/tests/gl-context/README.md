# GL resource ownership

Exercises the production GL resource state shared by contexts, textures, shader
programs and buffers. Requires a host C++ compiler.

```sh
bash Tools/Symbian/tests/gl-context/run.sh
```

The fixture checks context restoration, resource deletion and deferred cleanup.
Shared object deletions follow the EGL sharing group across compositor teardown;
framebuffer deletion remains tied to its creating context. It also checks
that failed context restoration is propagated to the caller.
