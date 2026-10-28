#include "PKGeometry.h"

#include <math.h>
#include <string.h>

#define PK_EPS 1e-10
#define PK_COORD_LIMIT 1e9

static double cross(PKPoint a, PKPoint b, PKPoint c) {
    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

static PKStatus check_points(const PKPoint *p, size_t n) {
    size_t i;
    if (!p) return PK_INVALID_ARGUMENT;
    if (n < 3 || n > PK_MAX_POINTS) return PK_INVALID_COUNT;
    for (i=0; i<n; ++i)
        if (!isfinite(p[i].x) || !isfinite(p[i].y) ||
            fabs(p[i].x)>PK_COORD_LIMIT || fabs(p[i].y)>PK_COORD_LIMIT)
            return PK_INVALID_COORDINATE;
    return PK_OK;
}

PKStatus pk_bounds(const PKPoint *p, size_t n, PKBounds *b) {
    size_t i;
    PKStatus status;
    if (!b) return PK_INVALID_ARGUMENT;
    status=check_points(p,n);
    if (status!=PK_OK) return status;
    b->min_x=b->max_x=p[0].x; b->min_y=b->max_y=p[0].y;
    for (i=1;i<n;++i) {
        if (p[i].x<b->min_x) b->min_x=p[i].x;
        if (p[i].x>b->max_x) b->max_x=p[i].x;
        if (p[i].y<b->min_y) b->min_y=p[i].y;
        if (p[i].y>b->max_y) b->max_y=p[i].y;
    }
    return PK_OK;
}

double pk_area(const PKPoint *p, size_t n) {
    size_t i;
    long double sum=0;
    if (check_points(p,n)!=PK_OK) return NAN;
    /* Translation avoids cancellation when a small contour has a large offset. */
    for (i=1;i+1<n;++i)
        sum+=((long double)p[i].x-p[0].x)*((long double)p[i+1].y-p[0].y)
             -((long double)p[i].y-p[0].y)*((long double)p[i+1].x-p[0].x);
    return (double)(sum/2);
}

static PKStatus normalize(const PKPoint *p,size_t n,PKPoint *q) {
    PKBounds b;
    PKStatus s=pk_bounds(p,n,&b);
    size_t i;
    double scale;
    if (s!=PK_OK) return s;
    scale=fmax(b.max_x-b.min_x,b.max_y-b.min_y);
    if (!(scale>0)) return PK_DEGENERATE_POLYGON;
    for(i=0;i<n;++i) {
        q[i].x=(p[i].x-b.min_x)/scale;
        q[i].y=(p[i].y-b.min_y)/scale;
    }
    return PK_OK;
}

static int point_on_segment(PKPoint p,PKPoint a,PKPoint b) {
    return fabs(cross(a,b,p))<=PK_EPS &&
        p.x>=fmin(a.x,b.x)-PK_EPS && p.x<=fmax(a.x,b.x)+PK_EPS &&
        p.y>=fmin(a.y,b.y)-PK_EPS && p.y<=fmax(a.y,b.y)+PK_EPS;
}

static int segment_intersects(PKPoint a,PKPoint b,PKPoint c,PKPoint d) {
    double c1=cross(a,b,c),c2=cross(a,b,d),c3=cross(c,d,a),c4=cross(c,d,b);
    if (((c1>PK_EPS && c2<-PK_EPS)||(c1<-PK_EPS && c2>PK_EPS)) &&
        ((c3>PK_EPS && c4<-PK_EPS)||(c3<-PK_EPS && c4>PK_EPS))) return 1;
    return point_on_segment(c,a,b)||point_on_segment(d,a,b)||
           point_on_segment(a,c,d)||point_on_segment(b,c,d);
}

PKStatus pk_validate_polygon(const PKPoint *p,size_t n) {
    PKPoint q[PK_MAX_POINTS];
    PKStatus s=normalize(p,n,q);
    size_t i,j;
    if(s!=PK_OK) return s;
    for(i=0;i<n;++i) {
        PKPoint a=q[(i+n-1)%n], b=q[i], c=q[(i+1)%n];
        for(j=i+1;j<n;++j)
            if(hypot(q[i].x-q[j].x,q[i].y-q[j].y)<=PK_EPS)
                return PK_DUPLICATE_POINT;
        /* Adjacent collinear edges that reverse direction overlap. */
        if(fabs(cross(a,b,c))<=PK_EPS &&
           (b.x-a.x)*(c.x-b.x)+(b.y-a.y)*(c.y-b.y)<-PK_EPS)
            return PK_SELF_INTERSECTION;
        for(j=i+1;j<n;++j) {
            size_t i1=(i+1)%n,j1=(j+1)%n;
            if(i1==j||j1==i) continue;
            if(segment_intersects(q[i],q[i1],q[j],q[j1]))
                return PK_SELF_INTERSECTION;
        }
    }
    if(fabs(pk_area(q,n))<=PK_EPS) return PK_DEGENERATE_POLYGON;
    return PK_OK;
}

/* Keep a single consistent simplified boundary for caps AND side walls.
 * Otherwise collinear input would create T-junctions in an exported STL. */
static size_t boundary(const PKPoint *q,size_t n,size_t *idx) {
    size_t m=n,i;
    int changed=1;
    for(i=0;i<n;++i) idx[i]=i;
    while(changed && m>3) {
        changed=0;
        for(i=0;i<m;++i) {
            PKPoint a=q[idx[(i+m-1)%m]],b=q[idx[i]],c=q[idx[(i+1)%m]];
            if(fabs(cross(a,b,c))<=PK_EPS && point_on_segment(b,a,c)) {
                memmove(idx+i,idx+i+1,(m-i-1)*sizeof(*idx));
                --m;changed=1;break;
            }
        }
    }
    return m;
}

static int inside_triangle(PKPoint p,PKPoint a,PKPoint b,PKPoint c) {
    return cross(a,b,p)>=-PK_EPS && cross(b,c,p)>=-PK_EPS && cross(c,a,p)>=-PK_EPS;
}

PKStatus pk_triangulate(const PKPoint *p,size_t n,PKTriangle *triangles,
                       size_t capacity,size_t *written) {
    PKPoint q[PK_MAX_POINTS];
    size_t idx[PK_MAX_POINTS],m,i,j,out=0;
    PKStatus s;
    if(!written) return PK_INVALID_ARGUMENT;
    *written=0;
    s=pk_validate_polygon(p,n);
    if(s!=PK_OK) return s;
    (void)normalize(p,n,q);
    m=boundary(q,n,idx);
    if(m<3) return PK_DEGENERATE_POLYGON;
    if(!triangles || capacity<m-2) {
        *written=m-2;
        return PK_INSUFFICIENT_CAPACITY;
    }
    if(pk_area(q,n)<0) {
        for(i=0;i<m/2;++i) {size_t t=idx[i];idx[i]=idx[m-1-i];idx[m-1-i]=t;}
    }
    while(m>3) {
        int found=0;
        for(i=0;i<m;++i) {
            size_t a=idx[(i+m-1)%m],b=idx[i],c=idx[(i+1)%m];
            int blocked=0;
            if(cross(q[a],q[b],q[c])<=PK_EPS) continue;
            for(j=0;j<m;++j) {
                size_t v=idx[j];
                if(v==a||v==b||v==c) continue;
                if(inside_triangle(q[v],q[a],q[b],q[c])) {blocked=1;break;}
            }
            if(blocked) continue;
            triangles[out++]=(PKTriangle){a,b,c};
            memmove(idx+i,idx+i+1,(m-i-1)*sizeof(*idx));
            --m;found=1;break;
        }
        if(!found) return PK_TRIANGULATION_FAILED;
    }
    if(cross(q[idx[0]],q[idx[1]],q[idx[2]])<=PK_EPS) return PK_TRIANGULATION_FAILED;
    triangles[out++]=(PKTriangle){idx[0],idx[1],idx[2]};
    *written=out;
    return PK_OK;
}

PKStatus pk_write_svg(FILE *stream,const PKPoint *p,size_t n) {
    PKBounds b;
    size_t i;
    PKStatus s;
    if(!stream) return PK_INVALID_ARGUMENT;
    s=pk_validate_polygon(p,n);
    if(s!=PK_OK) return s;
    (void)pk_bounds(p,n,&b);
    if(fprintf(stream,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.17gmm\" height=\"%.17gmm\" "
        "viewBox=\"%.17g %.17g %.17g %.17g\">\n"
        "<title>PRIV Key — contour mesuré, unités millimètres</title>\n"
        "<desc>Silhouette uniquement ; les profils et rainures invisibles ne sont pas reconstruits.</desc>\n"
        "<polygon fill=\"none\" stroke=\"black\" stroke-width=\"0.05\" points=\"",
        b.max_x-b.min_x,b.max_y-b.min_y,b.min_x,b.min_y,b.max_x-b.min_x,b.max_y-b.min_y)<0)
        return PK_IO_ERROR;
    for(i=0;i<n;++i)
        if(fprintf(stream,"%s%.17g,%.17g",i?" ":"",p[i].x,p[i].y)<0) return PK_IO_ERROR;
    if(fprintf(stream,"\"/>\n</svg>\n")<0 || fflush(stream)!=0 || ferror(stream)) return PK_IO_ERROR;
    return PK_OK;
}

typedef struct {double x,y,z;} PKVertex3;
static int facet(FILE *stream,PKVertex3 a,PKVertex3 b,PKVertex3 c) {
    double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
    double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
    double len=sqrt(nx*nx+ny*ny+nz*nz);
    if(!(len>0) || !isfinite(len)) return 0;
    return fprintf(stream,
        "  facet normal %.17g %.17g %.17g\n    outer loop\n"
        "      vertex %.17g %.17g %.17g\n      vertex %.17g %.17g %.17g\n"
        "      vertex %.17g %.17g %.17g\n    endloop\n  endfacet\n",
        nx/len,ny/len,nz/len,a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z)>=0;
}

PKStatus pk_write_stl(FILE *stream,const PKPoint *p,size_t n,double thickness) {
    PKPoint q[PK_MAX_POINTS];
    PKTriangle triangles[PK_MAX_POINTS-2];
    size_t idx[PK_MAX_POINTS],written,m,i;
    PKStatus s;
    if(!stream) return PK_INVALID_ARGUMENT;
    if(!isfinite(thickness) || thickness<=0 || thickness>PK_COORD_LIMIT) return PK_INVALID_THICKNESS;
    s=pk_triangulate(p,n,triangles,PK_MAX_POINTS-2,&written);
    if(s!=PK_OK) return s;
    (void)normalize(p,n,q);
    m=boundary(q,n,idx);
    if(pk_area(q,n)<0)
        for(i=0;i<m/2;++i) {size_t t=idx[i];idx[i]=idx[m-1-i];idx[m-1-i]=t;}
    if(fprintf(stream,"solid PRIV_Key_silhouette_mm\n")<0) return PK_IO_ERROR;
    for(i=0;i<written;++i) {
        PKTriangle t=triangles[i];
        PKVertex3 a={p[t.a].x,p[t.a].y,0},b={p[t.b].x,p[t.b].y,0},c={p[t.c].x,p[t.c].y,0};
        if(!facet(stream,c,b,a)) return PK_IO_ERROR;
        a.z=b.z=c.z=thickness;
        if(!facet(stream,a,b,c)) return PK_IO_ERROR;
    }
    for(i=0;i<m;++i) {
        PKPoint a=p[idx[i]],b=p[idx[(i+1)%m]];
        PKVertex3 a0={a.x,a.y,0},b0={b.x,b.y,0},a1={a.x,a.y,thickness},b1={b.x,b.y,thickness};
        if(!facet(stream,a0,b0,b1)||!facet(stream,a0,b1,a1)) return PK_IO_ERROR;
    }
    if(fprintf(stream,"endsolid PRIV_Key_silhouette_mm\n")<0 || fflush(stream)!=0 || ferror(stream))
        return PK_IO_ERROR;
    return PK_OK;
}

const char *pk_status_message(PKStatus status) {
    switch(status) {
        case PK_OK:return "Contour valide.";
        case PK_INVALID_ARGUMENT:return "Données manquantes.";
        case PK_INVALID_COUNT:return "Le contour doit contenir de 3 à 256 points.";
        case PK_INVALID_COORDINATE:return "Une coordonnée est invalide ou hors plage.";
        case PK_DUPLICATE_POINT:return "Deux points sont superposés ; supprimer le doublon.";
        case PK_SELF_INTERSECTION:return "Le contour se croise, se touche ou se recouvre.";
        case PK_DEGENERATE_POLYGON:return "Le contour est plat ou trop fin pour être exporté.";
        case PK_INSUFFICIENT_CAPACITY:return "Espace insuffisant pour la triangulation.";
        case PK_INVALID_THICKNESS:return "L'épaisseur doit être positive et finie.";
        case PK_IO_ERROR:return "Impossible d'écrire le fichier.";
        case PK_TRIANGULATION_FAILED:return "Le contour n'a pas pu être triangulé.";
        default:return "Erreur géométrique inconnue.";
    }
}
