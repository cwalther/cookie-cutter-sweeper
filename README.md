# Cookie Cutter Sweeper

1. A command-line program that sweeps (extrudes) a 2D cross-section shape along the edge of a 2D shape, both given as black and white PNG files, and exports the resulting solid as a triangle mesh in STL format.
2. An Inkscape extension using this for simple creation of 3D-printed cookie cutters from a black and white SVG shape.

See [http://zurich.fablab.ch/2013/12/nicer-cookie-cutters/](http://zurich.fablab.ch/2013/12/nicer-cookie-cutters/) for a description of the algorithm and development history.

## Installing the Inkscape Extension

- Get the zip file from the [releases page](../../releases).
- Follow the [Readme](inkscape/readme.txt) file for instructions on how to add the extension to Inkscape.

## Compiling the source code

On the [releases page](https://github.com/cwalther/cookie-cutter-sweeper/releases) you can find binaries for most systems as part of the Inkscape extension.

If you want to compile yourself the c++ part of the plugin, install libpng development files in a place where `pkg-config` finds them and run

```sh
make
```

(or examine and adapt the makefile, it’s trivial).
