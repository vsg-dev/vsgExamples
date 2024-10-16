## vsgtriangles

This example draws many triangles in one or more draw commands and in one or more
graphics pipelines. The scene graph is built rather pedantically from the basic
building blocks provided by VSG to facilitate experimentation.

By default, 1000 triangles will be rendered using a single draw call in a regular
grid in (x, y, z) in the range [-100, 100] for each dimension. The following command
shows 1M triangles with 10 pipelines, each with 10 separate draw commands:

```
vsgtriangles -p 10 -c 10 -n 100 100 100
```
