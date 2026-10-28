#include "../native/PRIVKey/PKGeometry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double x,y,z; } V3;
typedef struct {V3 v[3];} Tri;
static int near(double a,double b) {return fabs(a-b)<1e-8;}
static int same(V3 a,V3 b) {return near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z);}

static void mesh_check(const PKPoint *p,size_t n,double thickness,size_t expected_triangles) {
    FILE *f=tmpfile();
    char line[256];
    Tri triangles[1024];
    size_t vertex=0,nt=0,i,j,k,l;
    double volume=0;
    assert(f);
    assert(pk_write_stl(f,p,n,thickness)==PK_OK);
    rewind(f);
    while(fgets(line,sizeof(line),f)) {
        V3 v;
        if(sscanf(line," vertex %lf %lf %lf",&v.x,&v.y,&v.z)==3) {
            assert(nt<1024);
            triangles[nt].v[vertex++]=v;
            if(vertex==3) {vertex=0;++nt;}
        }
    }
    assert(vertex==0 && nt==expected_triangles);
    fclose(f);
    for(i=0;i<nt;++i) {
        V3 a=triangles[i].v[0],b=triangles[i].v[1],c=triangles[i].v[2];
        volume+=(a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x))/6;
        /* Every oriented edge has exactly one reverse partner. */
        for(j=0;j<3;++j) {
            V3 e1=triangles[i].v[j],e2=triangles[i].v[(j+1)%3];
            size_t matches=0,reverse=0;
            for(k=0;k<nt;++k) for(l=0;l<3;++l) {
                V3 a2=triangles[k].v[l],b2=triangles[k].v[(l+1)%3];
                if(same(e1,a2)&&same(e2,b2)) ++matches;
                if(same(e1,b2)&&same(e2,a2)) {++matches;++reverse;}
            }
            assert(matches==2 && reverse==1);
        }
    }
    assert(near(volume,fabs(pk_area(p,n))*thickness));
}

int main(void) {
    PKPoint box[]={{0,0},{4,0},{4,3},{0,3}};
    PKPoint concave[]={{0,0},{3,0},{3,1},{1,1},{1,3},{0,3}};
    PKPoint reverse[6];
    PKPoint collinear[]={{0,0},{2,0},{4,0},{4,3},{0,3}};
    PKPoint crossing[]={{0,0},{4,4},{0,4},{4,0}};
    PKPoint touching[]={{0,0},{4,0},{4,4},{2,0},{0,4}};
    PKPoint overlapping[]={{0,0},{4,0},{2,0},{2,3},{0,3}};
    PKPoint dup[]={{0,0},{4,0},{4,3},{0,0}};
    PKPoint flat[]={{0,0},{1,0},{2,0}};
    PKPoint invalid[]={{0,0},{NAN,0},{0,3}};
    PKPoint huge[]={{1e10,0},{1e10,2},{0,3}};
    PKTriangle tris[254];
    PKBounds bounds;
    size_t written=999,i;
    FILE *f;
    char svg[4096];
    assert(pk_validate_polygon(box,4)==PK_OK);
    assert(pk_validate_polygon(concave,6)==PK_OK);
    assert(pk_bounds(box,4,&bounds)==PK_OK && near(bounds.max_x,4) && near(bounds.max_y,3));
    assert(near(pk_area(box,4),12));
    assert(near(pk_area(concave,6),5));
    assert(pk_triangulate(box,4,tris,254,&written)==PK_OK && written==2);
    assert(pk_triangulate(box,4,tris,1,&written)==PK_INSUFFICIENT_CAPACITY && written==2);
    assert(pk_triangulate(box,4,NULL,0,&written)==PK_INSUFFICIENT_CAPACITY && written==2);
    assert(pk_validate_polygon(crossing,4)==PK_SELF_INTERSECTION);
    assert(pk_validate_polygon(touching,5)==PK_SELF_INTERSECTION);
    assert(pk_validate_polygon(overlapping,5)==PK_SELF_INTERSECTION);
    assert(pk_validate_polygon(dup,4)==PK_DUPLICATE_POINT);
    assert(pk_validate_polygon(flat,3)!=PK_OK);
    assert(pk_validate_polygon(invalid,3)==PK_INVALID_COORDINATE);
    assert(pk_validate_polygon(huge,3)==PK_INVALID_COORDINATE);
    assert(pk_validate_polygon(NULL,3)==PK_INVALID_ARGUMENT);
    assert(pk_validate_polygon(box,2)==PK_INVALID_COUNT);
    assert(pk_validate_polygon(box,257)==PK_INVALID_COUNT);
    assert(pk_validate_polygon(collinear,5)==PK_OK);
    assert(pk_triangulate(collinear,5,tris,254,&written)==PK_OK && written==2);
    for(i=0;i<6;++i) reverse[i]=concave[5-i];
    assert(near(pk_area(reverse,6),-5));
    mesh_check(box,4,2,12);
    mesh_check(concave,6,2,20);
    mesh_check(reverse,6,2,20);
    mesh_check(collinear,5,2,12);
    f=tmpfile();assert(f);
    assert(pk_write_stl(f,box,4,0)==PK_INVALID_THICKNESS);
    assert(pk_write_stl(f,box,4,-1)==PK_INVALID_THICKNESS);
    assert(pk_write_stl(f,box,4,NAN)==PK_INVALID_THICKNESS);
    assert(pk_write_stl(f,box,4,INFINITY)==PK_INVALID_THICKNESS);
    assert(pk_write_stl(NULL,box,4,1)==PK_INVALID_ARGUMENT);
    assert(pk_write_svg(f,box,4)==PK_OK);
    rewind(f);i=fread(svg,1,sizeof(svg)-1,f);svg[i]=0;
    assert(strstr(svg,"width=\"4mm\"") && strstr(svg,"height=\"3mm\""));
    assert(strstr(svg,"viewBox=\"0 0 4 3\""));
    fclose(f);
    f=fopen("/dev/full","w");
    if(f) {assert(pk_write_svg(f,box,4)==PK_IO_ERROR);fclose(f);}
    puts("PKGeometry: validation, concave triangulation, both windings, collinear simplification, watertight edge topology, signed volume, SVG units and I/O checks passed.");
    return 0;
}
