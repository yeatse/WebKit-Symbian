# GL resource ownership

Exercises the production GL resource state shared by contexts, textures, shader
programs and buffers. Requires a host C++ compiler.

```sh
bash Tools/Symbian/tests/gl-context/run.sh
```

The fixture checks context restoration, resource deletion and deferred cleanup.
It exits successfully when each resource is released through its owning context
and failed context restoration is propagated to its caller.
