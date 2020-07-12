//
//  sweep.cpp
//
//  Copyright (c) 2013 Christian Walther
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include <png.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <map>

struct Offset {
	int dx;
	int dy;
	float dist;
	Offset *next;
};

int offsetCompare(const void *o1, const void *o2) {
	float c = ((Offset *)o1)->dist - ((Offset *)o2)->dist;
	return (c < 0) ? -1 : (c > 0) ? 1 : 0;
}

// workarounds for the fact that a float can't be passed as a non-type template parameter
struct OutsideShortFFFF { static const unsigned short value; };
const unsigned short OutsideShortFFFF::value = 0xFFFF;
struct OutsideFloatMax { static const float value; };
const float OutsideFloatMax::value = FLT_MAX;

template <typename T, typename Outside> class Matrix {
private:
	struct Content {
		int width, height;
		float scale; // cells per millimeter
		int refcount;
		T data[0];
	} *content;
public:
	Matrix(int width, int height)
		: content(reinterpret_cast<Content*>(new unsigned char[sizeof(Content) + width*height*sizeof(T)]))
	{
		content->width = width;
		content->height = height;
		content->scale = 1.0f;
		content->refcount = 1;
	}
	Matrix(const Matrix& other)
		: content(other.content)
	{
		content->refcount++;
	}
	~Matrix() {
		if (--content->refcount == 0) {
			delete[] content;
		}
	}
	int getWidth() const {
		return content->width;
	}
	int getHeight() const {
		return content->height;
	}
	void setScale(float scale) {
		content->scale = scale;
	}
	float getScale() const {
		return content->scale;
	}
	T *getPointer() const {
		return content->data;
	}
	T *getRowPointer(int y) const {
		return &content->data[y*content->width];
	}
	T get(int x, int y) const {
		if (x < 0 || y < 0 || x >= content->width || y >= content->height) return Outside::value;
		else return content->data[y*content->width + x];
	}
	void put(int x, int y, T value) {
		if (x < 0 || y < 0 || x >= content->width || y >= content->height) return;
		else content->data[y*content->width + x] = value;
	}
};
typedef Matrix<unsigned short, OutsideShortFFFF> PixelMatrix;
typedef Matrix<float, OutsideFloatMax> FloatMatrix;

struct Vertex {
	float xsum, ysum, zsum;
	int count;
	Vertex() : xsum(0), ysum(0), zsum(0), count(0) {}
	void normalize() {
		xsum /= count;
		ysum /= count;
		zsum /= count;
	}
};

struct Cell {
	int x, y, z;
	Cell(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
	bool operator<(const Cell& other) const {
		if (z < other.z) return true;
		else if (other.z < z) return false;
		else if (y < other.y) return true;
		else if (other.y < y) return false;
		else if (x < other.x) return true;
		else return false;
	}
};

struct Face {
	unsigned int index[4];
};

PixelMatrix readPng(const char *filename) {
	png_structp png;
	png_infop pnginfo;
	png_uint_32 w, h;
	png_uint_32 resolution;
	int bitdepth, colortype;
	int i;
	FILE *fp;
	png_size_t rowbytes;
	png_bytep *rows = NULL;
	static const png_color_16 white = {0xFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

	if (strcmp(filename, "-") == 0) {
		fp = stdin;
	}
	else {
		fp = fopen(filename, "rb");
	}
	if (fp == NULL) {
		std::ostringstream msg;
		msg << "cannot open file " << filename << ": " << strerror(errno);
		throw std::runtime_error(msg.str());
	}

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) {
		if (fp != stdin) fclose(fp);
		throw std::runtime_error("cannot create PNG read structure");
	}

	pnginfo = png_create_info_struct(png);
	if (pnginfo == NULL) {
		png_destroy_read_struct(&png, NULL, NULL);
		if (fp != stdin) fclose(fp);
		throw std::runtime_error("cannot create PNG info structure");
	}

	if (setjmp(png_jmpbuf(png))) {
		if (rows) png_free(png, rows);
		png_destroy_read_struct(&png, &pnginfo, NULL);
		if (fp != stdin) fclose(fp);
		// libpng already printed its error message
		throw std::runtime_error("PNG reading error");
	}

	png_init_io(png, fp);

	png_set_alpha_mode(png, PNG_ALPHA_STANDARD, PNG_DEFAULT_sRGB);

	png_read_info(png, pnginfo);
	png_get_IHDR(png, pnginfo, &w, &h, &bitdepth, &colortype, NULL, NULL, NULL);

//	if (colortype == PNG_COLOR_TYPE_PALETTE) { // does this work for grayscale output?
//		png_set_palette_to_rgb(png);
//	}
//	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8) {
//		png_set_expand_gray_1_2_4_to_8(png);
//	}
	png_set_expand_16(png); //this may make the previous two redundant
	if (bitdepth < 8) png_set_packing(png);
	if (colortype == PNG_COLOR_TYPE_RGB || colortype == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_rgb_to_gray(png, 1, PNG_RGB_TO_GRAY_DEFAULT, PNG_RGB_TO_GRAY_DEFAULT);
	}
	png_set_background(png, &white, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1);
#ifndef WORDS_BIGENDIAN
	png_set_swap(png);
#endif
	png_read_update_info(png, pnginfo);

	rowbytes = png_get_rowbytes(png, pnginfo);
	if (rowbytes != 2*w) {
		png_destroy_read_struct(&png, &pnginfo, NULL);
		if (fp != stdin) fclose(fp);
		throw std::runtime_error("unexpected pixel format");
	}
	rows = (png_bytep *)png_malloc(png, h*sizeof(png_bytep));
	PixelMatrix pix(w, h);
	for (i = 0; i < h; i++) {
		rows[i] = (png_bytep)pix.getRowPointer(i);
	}

	png_read_image(png, rows);

	png_read_end(png, NULL);

	png_free(png, rows);
	
	resolution = png_get_pixels_per_meter(png, pnginfo);
	if (resolution != 0) {
		pix.setScale(resolution/1000.0f);
	}
	
	png_destroy_read_struct(&png, &pnginfo, NULL);
	if (fp != stdin) fclose(fp);

	return pix;
}

FloatMatrix distanceField(const PixelMatrix& pixels, int maxRadius) {
	int i, j;
	int x, y;
	int diam = 2*maxRadius + 1;

	// set up a data structure to iterate through the pixels surrounding a starting point, up to the maximum radius, ordered by distance
	// (Is there a way of doing this algorithmically, without storing it all in memory and then sorting? I couldn't think of any.)
	Offset *o;
	Offset *no;
	Offset *offsets;
	Offset *offsetsend;
	offsets = new Offset[diam*diam];
	for (i = 0; i < diam; i++) {
		for (j = 0; j < diam; j++) {
			o = &offsets[i*diam + j];
			int dx = j - maxRadius;
			int dy = i - maxRadius;
			o->dx = dx;
			o->dy = dy;
			if (dx == 0 && dy == 0) {
				// we don't need an entry for (0, 0), so make sure it gets sorted to the end
				o->dist = 2*maxRadius;
			}
			else {
				o->dist = sqrtf(dx*dx + dy*dy);
			}
		}
	}
	qsort(offsets, diam*diam, sizeof(Offset), &offsetCompare);
	// figure out
	// - where to start for the next pixel: it can't be more than one pixel closer
	// - where to stop: at the radius (cutting off the corners of the square), otherwise the above optimization doesn't work
	for (o = offsets, no = offsets; ; o++) {
		while (no->dist < o->dist - 1.0f) no++;
		if (no->dist > maxRadius) {
			offsetsend = o;
			break;
		}
		o->next = no;
	}
	offsetsend->next = (offsetsend-1)->next;

	// now scan the image
	FloatMatrix field(pixels.getWidth() + 2*maxRadius, pixels.getHeight() + 2*maxRadius);
	field.setScale(pixels.getScale());
	o = offsets;
	x = -1;
	for (y = 0; y < field.getHeight(); y++) {
		// alternate the x direction (serpentine scan) so that the distance to the previous pixel is never more than 1 pixel
		int dx = ((y & 1) == 0) ? 1 : -1;
		x += dx;
		for (; x >= 0 && x < field.getWidth(); x += dx) {
			int ix = x - maxRadius;
			int iy = y - maxRadius;
			unsigned short p = pixels.get(ix, iy);
			unsigned short p2;
			float d = 2*maxRadius;
			unsigned short sign = (p & 0x8000);
			no = NULL;
			for (; o < offsetsend && o->dist < d + 1.41421356237f; o++) {
				ix = x - maxRadius + o->dx;
				iy = y - maxRadius + o->dy;
				p = pixels.get(ix, iy);
				if ((unsigned short)(p & 0x8000) != sign) {
					for (i = 0; i < 4; i++) {
						int dx2 = (((i & 2) + ((i<<1) & 2) - 2) >> 1); // -1, 0, 0, 1
						int dy2 = (((i & 2) - ((i<<1) & 2)) >> 1); // 0, -1, 1, 0
						p2 = pixels.get(ix + dx2, iy + dy2);
						if ((unsigned short)(p2 & 0x8000) == sign) {
							// Grid edge crosses the shape edge. Two cases to compute the intersection point:
							// - If one end of the grid edge is totally white or black, assume that this is a shape edge near perpendicular to the grid line and properly antialiased by area coverage, where the grid line goes through black pixels, then at most one gray pixel, then white pixels. In that case we can reconstruct the intersection point more accurately than by taking the 0.5-isoline of the bilinear interpolation. The latter is an inferior approximation and would lead to visible aliasing (rippled surface) in the end product.
							// - Else: a near-diagonal edge, a near-parallel edge to the grid line where accuracy of the intersection point makes little difference, no single straight edge at all, or improperly antialiased (too blurry) - fall back to taking the 0.5-isoline of the bilinear interpolation.
							// The two formulas happen to coincide along the lines u - v = +-0.5, so those are convenient boundaries for an overall continuous formula.
							// From a purely visual comparison, in fact it seems like this combined formula for the most part of the parameter space precisely matches the exact solution for a straight edge of any slope, but I haven't done all the math yet.
							float dc;
							float u = (float)p2/65535.0f;
							float v = (float)p/65535.0f;
							if (u - v <= -0.5f) {
								// by area coverage for u = 0 or v = 1
								dc = -0.5f + u + v;
							}
							else if (u - v >= 0.5f) {
								// by area coverage for u = 1 or v = 0
								dc = 1.5f - u - v;
							}
							else {
								// 0.5-isoline of bilinear interpolation (just linear interpolation here since we're on a grid edge)
								dc = (0.5f - v)/(u - v);
							}
							float dx = o->dx + dx2*dc;
							float dy = o->dy + dy2*dc;
							dc = sqrtf(dx*dx + dy*dy);
							if (dc < d) d = dc;
						}
					}
					if (no == NULL) no = o;
				}
			}
			if (no == NULL) no = o;
			o = no->next;
			field.put(x, y, (sign != 0) ? d : -d);
		}
	}
	delete[] offsets;
	return field;
}

void writeFloat(float v, FILE *f) {
	union { float fv; uint32_t uv; };
	fv = v;
#ifdef WORDS_BIGENDIAN
	uv = (((uv >> 24) & 0xFF) | ((uv >> 8) & 0xFF00) | ((uv << 8) & 0xFF0000) | (uv << 24));
#endif
	fwrite(&uv, 4, 1, f);
}

void writeStlTriangle(const Vertex& v1, const Vertex& v2, const Vertex& v3, FILE *f) {
	float l1x = v2.xsum - v1.xsum;
	float l1y = v2.ysum - v1.ysum;
	float l1z = v2.zsum - v1.zsum;
	float l2x = v3.xsum - v1.xsum;
	float l2y = v3.ysum - v1.ysum;
	float l2z = v3.zsum - v1.zsum;
	float nx = l1y*l2z - l2y*l1z;
	float ny = l1z*l2x - l2z*l1x;
	float nz = l1x*l2y - l2x*l1y;
	float nn = sqrtf(nx*nx + ny*ny + nz*nz);
	writeFloat(nx/nn, f);
	writeFloat(ny/nn, f);
	writeFloat(nz/nn, f);
	writeFloat(v1.xsum, f);
	writeFloat(v1.ysum, f);
	writeFloat(v1.zsum, f);
	writeFloat(v2.xsum, f);
	writeFloat(v2.ysum, f);
	writeFloat(v2.zsum, f);
	writeFloat(v3.xsum, f);
	writeFloat(v3.ysum, f);
	writeFloat(v3.zsum, f);
	fputc(0, f);
	fputc(0, f);
}

int main(int argc, char **argv) {
	const char* filenames[3] = {NULL, NULL, NULL};
	bool flipX = false;
	for (int i = 1, j = 0; i < argc; i++) {
		if (strcmp(argv[i], "--flip-x") == 0) {
			flipX = !flipX;
		}
		else {
			if (j < 3) filenames[j++] = argv[i];
		}
	}

	if (filenames[2] == NULL) {
		fprintf(stderr, "Usage: %s [--flip-x] <crosssection.png> <shape.png> <output.stl>\n\n"
			"Sweep (extrude) a cross-section shape along the edge of a shape to make a solid.\n"
			"Input: black = inside, white or transparent = outside, antialiasing recommended.\n"
			"Left half of cross section goes inside, right half outside.\n"
			"Output is scaled according to resolution of cross-section image.\n"
			"--flip-x: Mirror output in x direction.\n\n"
			"Version 1.3\n"
			"Copyright (c) 2013-2020 Christian Walther <cwalther%cgmx.ch>\n"
			"https://github.com/cwalther/cookie-cutter-sweeper\n", argv[0], '@');
		return 2;
	}

	try {
		// compute the 2D distance fields of section and shape
		FloatMatrix section = distanceField(readPng(filenames[0]), 2);
		FloatMatrix shape = distanceField(readPng(filenames[1]), (section.getWidth() + 1)/2);

		// initialization for working in z-slices, keeping only the last two slices in memory
		FloatMatrix slice1(shape.getWidth(), shape.getHeight());
		FloatMatrix slice2(shape.getWidth(), shape.getHeight());
		FloatMatrix *thisSlice = &slice1;
		FloatMatrix *lastSlice = &slice2;
		for (int y = 0; y < shape.getHeight(); y++) {
			for (int x = 0; x < shape.getWidth(); x++) {
				lastSlice->put(x, y, FLT_MAX);
			}
		}

		// data structures for mesh generation
		std::map<Cell, unsigned int> verticesByCell;
		std::vector<Vertex> vertices;
		std::vector<Face> faces;

		// slice by slice:
		for (int z = 0; z < section.getHeight(); z++) {
			// compute the 3D distance field of the solid
			for (int y = 0; y < shape.getHeight(); y++) {
				for (int x = 0; x < shape.getWidth(); x++) {
					float d = shape.get(x, y) + ((float)section.getWidth())/2;
					float f = floorf(d);
					d -= f;
					int k = (int)f;
					thisSlice->put(x, y, (1.0f - d)*section.get(k, z) + d*section.get(k+1, z));
				}
			}

			// generate a quad mesh of its zero isosurface using a "naive surface nets" algorithm (http://0fps.wordpress.com/2012/07/12/smooth-voxel-terrain-part-2/)
			for (int y = 1; y < shape.getHeight(); y++) {
				for (int x = 1; x < shape.getWidth(); x++) {
					float p = thisSlice->get(x, y);
					// for all three edges from (x, y, z) in negative direction
					for (int i = 0; i < 3; i++) {
						int dx = (i == 0), dy = (i == 1), dz = (i == 2);
						float q = ((dz == 0) ? thisSlice : lastSlice)->get(x-dx, y-dy);
						if ((p >= 0) != (q >= 0)) {
							// edge intersects surface, output a face
							// (we don't know yet exactly where its vertices are, but we know which ones (belonging to which cells) they are)
							faces.push_back(Face());
							Face& face = faces[faces.size()-1];
							// for all four cells surrounding the edge, in right-hand-rule order with respect to (dx, dy, dz), i.e. all four vertices of the face
							for (int j = 0; j < 4; j++) {
								int k = (j ^ (j >> 1));
								k = ((k << (1+i)) | (k << (4+i)));
								int ex = ((k >> 3) & 1);
								int ey = ((k >> 4) & 1);
								int ez = ((k >> 5) & 1);
								// find or create vertex
								unsigned int& vi = verticesByCell[Cell(x+ex, y+ey, z+ez)];
								if (vi == 0) {
									vertices.push_back(Vertex());
									vi = vertices.size(); // after push so it's index+1 because 0 is reserved for unoccupied
								}
								Vertex& v = vertices[vi-1];
								// accumulate vertex position data from this edge
								// (by the time we're through the next slice, it will have accumulated this from all edges of its cell)
								float d = p/(p - q);
								v.xsum += x - dx*d;
								v.ysum += y - dy*d;
								v.zsum += z - dz*d;
								v.count++;
								face.index[j] = vi - 1;
							}
							if ((q >= 0) != flipX) {
								// outwards-pointing edge, reverse face orientation
								unsigned int t = face.index[1];
								face.index[1] = face.index[3];
								face.index[3] = t;
							}
						}
					}
				}
			}

			// swap slice storage
			FloatMatrix *t = lastSlice;
			lastSlice = thisSlice;
			thisSlice = t;
		}

		// post-processing to find actual vertex positions
		for (std::vector<Vertex>::iterator it = vertices.begin(); it != vertices.end(); ++it) {
			// finish averaging
			it->normalize();
			// - invert in y and z directions to convert from upside-down image coordinate system
			// - invert in x direction if requested
			// - shift by (1, 1, 1) to ensure all coordinates are positive as required by STL spec
			// - scale by the resolution read from the section image, if any
			if (flipX) it->xsum = shape.getWidth() - it->xsum;
			it->xsum = (it->xsum + 1.0f)/section.getScale();
			it->ysum = (shape.getHeight() - it->ysum + 1.0f)/section.getScale();
			it->zsum = (section.getHeight() - it->zsum + 1.0f)/section.getScale();
		}

		// now we have a mesh in a typical list-of-vertex-coordinates + list-of-faces-by-vertex-indices form, save it in STL format, which only stores individual triangles
		FILE* of;
		if (strcmp(filenames[2], "-") == 0) {
			of = stdout;
		}
		else {
			of = fopen(filenames[2], "wb");
		}
		if (of == NULL) {
			fprintf(stderr, "cannot open file %s: %s\n", filenames[2], strerror(errno));
			return 1;
		}
		for (int i = 0; i < 80; i++) fputc(0, of);
		uint32_t n = 2*faces.size();
#ifdef WORDS_BIGENDIAN
		n = (((n >> 24) & 0xFF) | ((n >> 8) & 0xFF00) | ((n << 8) & 0xFF0000) | (n << 24));
#endif
		fwrite(&n, 4, 1, of);
		for (std::vector<Face>::iterator it = faces.begin(); it != faces.end(); ++it) {
			// left as an exercise for the reader: find the better diagonal to split the quad along
			writeStlTriangle(vertices[it->index[0]], vertices[it->index[1]], vertices[it->index[2]], of);
			writeStlTriangle(vertices[it->index[0]], vertices[it->index[2]], vertices[it->index[3]], of);
		}
		if (of != stdout) {
			fclose(of);
		}
	}
	catch (const std::exception& e) {
		fprintf(stderr, "%s\n", e.what());
		return 1;
	}
	return 0;
}
