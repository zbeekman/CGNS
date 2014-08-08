/*
! @file benchmark_hdf5.F90
! @author M. Scot Breitenfeld <brtnfld@hdfgroup.org>
! @version 0.1
!
! @section LICENSE
! BSD style license
!
! @section DESCRIPTION
! Benchmarking program for pcgns library
!
! TO COMPILE: h5pcc -O2 benchmark_hdf5.c -I.. -L../lib -lcgns
!
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pcgnslib.h"
#include "mpi.h"

#define false 0
#define true 1

int comm_size;
int comm_rank;
MPI_Info info;

/* cgsize_t Nelem = 33554432; */
cgsize_t Nelem = 2097152;
cgsize_t NodePerElem = 6;

cgsize_t Nnodes;
int mpi_err;
int err;
int comm_size;
int comm_rank;
int info;
int fn;
int B;
int Z;
int S;
int Cx,Cy,Cz, Fx, Fy, Fz, Ar, Ai;
int cell_dim = 3;
int phys_dim = 3;
int r_cell_dim = 0;
int r_phys_dim = 0;
cgsize_t nijk[3], sizes[3];
cgsize_t size_1D[1];
cgsize_t min, max;
int k, count;
/* For writing and reading data*/
double* Coor_x;
double* Coor_y;
double* Coor_z;
double* Data_Fx;
double* Data_Fy;
double* Data_Fz;
double* Array_r;
cgsize_t* Array_i;
cgsize_t start, end, emin, emax;
cgsize_t* elements;
char name[33];
int queue, debug;
double t0, t1, t2;

/*
 * Timing storage convention:
 * timing(0) = Total program time
 * timing(1) = Time to write nodal coordinates
 * timing(2) = Time to write connectivity table
 * timing(3) = Time to write solution data (field data)
 * timing(4) = Time to write array data
 * timing(5) = Time to read nodal coordinates
 * timing(6) = Time to read connectivity table
 * timing(7) = Time to read solution data (field data)
 * timing(8) = Time to read array data 
 */
double xtiming[9], timing[9], timingMin[9], timingMax[9];

/*   ! CGP_INDEPENDENT is the default */
/*   INT, DIMENSION(1:2) :: piomode = (/CGP_INDEPENDENT, CGP_COLLECTIVE/) */
/* static char *piomode[2] = {"CGP_INDEPENDENT", "CGP_COLLECTIVE"}; */

static char *outmode[2] = {"direct", "queued"};
int piomode[2] = {0, 1};
int piomode_i;

int initialize(int* argc, char** argv[]) {
	int i,j;
	MPI_Init(argc,argv);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	MPI_Info_create(&info);
	MPI_Info_set(info, "striping_unit", "8388608") ;
	/* or whatever your GPFS block size actually is*/


	return 0;
}

int c_double_eq(double a, double b) {

  double eps = 1.e-8;
  
  if(a-b < eps) {
    return true;
  }
  return false;
}

int main(int argc, char* argv[]) {
  /* Initialize variables */
  initialize(&argc,&argv);

  char fname[32];
  char name[32];

  /* parameters */
  piomode_i = 1;
  queue = false;
  debug = false; 

  t0 = MPI_Wtime(); /* Timer */
  
  err = (int)cgp_pio_mode((CGNS_ENUMT(PIOmode_t))piomode_i, info);
  
  Nnodes = Nelem*NodePerElem;
  
  nijk[0] = Nnodes; /* Number of vertices */
  nijk[1] = Nelem; /* Number of cells */
  nijk[2] = 0; /* Number of boundary vertices */
  
  /* ====================================== */
  /* ==    **WRITE THE CGNS FILE *       == */
  /* ====================================== */

  /* for IBM */
  sprintf(fname, "benchmark_%06d_%d.cgns", comm_size, piomode_i+1);
/*   sprintf(fname, "benchmark_%06d_%d.cgns", comm_size, piomode_i+1); */
  if(cgp_open(fname, CG_MODE_WRITE, &fn) != CG_OK) {
    printf("*FAILED* cgp_open \n");
    cgp_error_exit();
  }
  if(cg_base_write(fn, "Base 1", cell_dim, phys_dim, &B) != CG_OK) {
    printf("*FAILED* cg_base_write \n");
    cgp_error_exit();
  }
  if(cg_zone_write(fn, B, "Zone 1", nijk, Unstructured, &Z) != CG_OK) {
    printf("*FAILED* cg_zone_write \n");
    cgp_error_exit();
  }
  /* use queued IO */
  if(cgp_queue_set(queue) != CG_OK) {
    printf("*FAILED* cgp_queue_set \n");
    cgp_error_exit();
  }

  /* ====================================== */
  /* == (A) WRITE THE NODAL COORDINATES  == */
  /* ====================================== */

  count = nijk[0]/comm_size;
  
  if( !(Coor_x = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_x \n");
    cgp_error_exit();
  }

  if( !(Coor_y= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_y \n");
    cgp_error_exit();
  }

  if( !(Coor_z= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_z \n");
    cgp_error_exit();
  }

  min = count*comm_rank+1;
  max = count*(comm_rank+1);
  
  for (k=0; k < count; k++) { 
    Coor_x[k] = comm_rank*count + k + 1.1;
    Coor_y[k] = Coor_x[k] + 0.1;
    Coor_z[k] = Coor_y[k] + 0.1;
  }

  if(cgp_coord_write(fn,B,Z,CGNS_ENUMV(RealDouble),"CoordinateX",&Cx) != CG_OK) {
    printf("*FAILED* cgp_coord_write (Coor_x) \n");
    cgp_error_exit();
  }
  if(cgp_coord_write(fn,B,Z,CGNS_ENUMV(RealDouble),"CoordinateY",&Cy) != CG_OK) {
    printf("*FAILED* cgp_coord_write (Coor_y) \n");
    cgp_error_exit();
  }
  if(cgp_coord_write(fn,B,Z,CGNS_ENUMV(RealDouble),"CoordinateZ",&Cz) != CG_OK) {
    printf("*FAILED* cgp_coord_write (Coor_z) \n");
    cgp_error_exit();
  }

  t1 = MPI_Wtime();
  if((cgp_coord_write_data(fn,B,Z,Cx,&min,&max,Coor_x)) != CG_OK) {
    printf("*FAILED* cgp_coord_write_data (Coor_x) \n");
    cgp_error_exit();
  }
  if((cgp_coord_write_data(fn,B,Z,Cy,&min,&max,Coor_y)) != CG_OK) {
    printf("*FAILED* cgp_coord_write_data (Coor_y) \n");
    cgp_error_exit();
  }
  if((cgp_coord_write_data(fn,B,Z,Cz,&min,&max,Coor_z)) != CG_OK) {
    printf("*FAILED* cgp_coord_write_data (Coor_z) \n");
    cgp_error_exit();
  }

  t2 = MPI_Wtime();
  xtiming[1] = t2-t1;

  if(!queue) {
    free(Coor_x);
    free(Coor_y);
    free(Coor_z);
  }
  /* ====================================== */
  /* == (B) WRITE THE CONNECTIVITY TABLE == */
  /* ====================================== */
  
  start = 1;
  end = nijk[1];

  if(cgp_section_write(fn,B,Z,"Elements",PENTA_6,start,end,0,&S) != CG_OK) {
    printf("*FAILED* cgp_section_write \n");
    cgp_error_exit();
  }
 
  count = nijk[1]/comm_size;

  if( !(elements = malloc(count*NodePerElem*sizeof(cgsize_t)) )) {
    printf("*FAILED* allocation of elements \n");
    cgp_error_exit();
  }
  
  /* Create ridiculous connectivity table ... */
  for ( k = 0; k < count*NodePerElem; k++) {
    elements[k] = comm_rank*count*NodePerElem + k + 1;
  }
  
  emin = count*comm_rank+1;
  emax = count*(comm_rank+1);

  t1 = MPI_Wtime();
  if(cgp_elements_write_data(fn, B, Z, S, emin, emax, elements) != CG_OK) {
    printf("*FAILED* cgp_elements_write_data (elements) \n");
    cgp_error_exit();
  }

  t2 = MPI_Wtime();
  xtiming[2] = t2-t1;

  if(!queue) {
    free(elements);
  }


  /* ====================================== */
  /* == (C) WRITE THE FIELD DATA         == */
  /* ====================================== */

  count = nijk[0]/comm_size;
    
  if( !(Data_Fx = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Data_Fx \n");
    cgp_error_exit();
  }

  if( !(Data_Fy= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Data_Fy \n");
    cgp_error_exit();
  }

  if( !(Data_Fz= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Data_Fz \n");
    cgp_error_exit();
  }

  for ( k = 0; k < count; k++) {
     Data_Fx[k] = comm_rank*count+k + 1.01;
     Data_Fy[k] = comm_rank*count+k + 1.02;
     Data_Fz[k] = comm_rank*count+k + 1.03;
  }

  if(cg_sol_write(fn, B, Z, "Solution", Vertex, &S) != CG_OK) {
    printf("*FAILED* cg_sol_write \n");
    cgp_error_exit();
  }

  if(cgp_field_write(fn,B,Z,S,CGNS_ENUMV(RealDouble),"MomentumX",&Fx) != CG_OK) {
    printf("*FAILED* cgp_field_write (MomentumX) \n");
    cgp_error_exit();
  }
  if(cgp_field_write(fn,B,Z,S,CGNS_ENUMV(RealDouble),"MomentumY",&Fy) != CG_OK) {
    printf("*FAILED* cgp_field_write (MomentumY) \n");
    cgp_error_exit();
  }
  if(cgp_field_write(fn,B,Z,S,CGNS_ENUMV(RealDouble),"MomentumZ",&Fz) != CG_OK) {
    printf("*FAILED* cgp_field_write (MomentumZ) \n");
    cgp_error_exit();
  }

  t1 = MPI_Wtime();
  if(cgp_field_write_data(fn,B,Z,S,Fx,&min,&max,Data_Fx) != CG_OK) {
    printf("*FAILED* cgp_field_write_data (Data_Fx) \n");
    cgp_error_exit();
  }
  if(cgp_field_write_data(fn,B,Z,S,Fy,&min,&max,Data_Fy) != CG_OK) {
    printf("*FAILED* cgp_field_write_data (Data_Fy)\n");
    cgp_error_exit();
  }
  if(cgp_field_write_data(fn,B,Z,S,Fz,&min,&max,Data_Fz) != CG_OK) {
    printf("*FAILED* cgp_field_write_data (Data_Fz)\n");
    cgp_error_exit();
  }

  t2 = MPI_Wtime();
  xtiming[3] = t2-t1;

  if(!queue) {
    free(Data_Fx);
    free(Data_Fy);
    free(Data_Fz);
  }

  /* skipping because the memory usage is not scalable (EXAHDF-65) */
  goto skip1;

  /* ====================================== */
  /* == (D) WRITE THE ARRAY DATA         == */
  /* ====================================== */

  count = nijk[0]/comm_size;

  if( !(Array_r = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Array_r \n");
    cgp_error_exit();
  }

  if( !(Array_i= (cgsize_t*) malloc(count*sizeof(cgsize_t))) ) {
    printf("*FAILED* allocation of Array_i  \n");
    cgp_error_exit();
  }

  min = count*comm_rank+1;
  max = count*(comm_rank+1);
  
  for ( k = 0; k < count; k++) {
    Array_r[k] = comm_rank*count + k + 1.001;
    Array_i[k] = comm_rank*count*NodePerElem + k + 1;
  }

  if(cg_goto(fn, B, "Zone 1", 0, "end") != CG_OK) {
    printf("*FAILED* cg_goto\n");
    cgp_error_exit();
  }

  if(cg_user_data_write("User Data") != CG_OK) {
    printf("*FAILED* cg_user_data_write \n");
    cgp_error_exit();
  }

  if(cg_gorel(fn,"User Data",0,"end") != CG_OK) {
    printf("*FAILED* cg_gorel\n");
    cgp_error_exit();
  }

   size_1D[0] = nijk[0];
  if(cgp_array_write("ArrayR",CGNS_ENUMV(RealDouble),1,size_1D,&Ar) != CG_OK) {
    printf("*FAILED* cgp_array_write (Array_Ar)\n");
    cgp_error_exit();
  }

#if CG_BUILD_64BIT
  if(cgp_array_write("ArrayI",CGNS_ENUMV(LongInteger),1,size_1D,&Ai) != CG_OK) {
    printf("*FAILED* cgp_array_write (Array_Ai)\n");
    cgp_error_exit();
  }
#else
  if(cgp_array_write("ArrayI",CGNS_ENUMV(Integer),1,size_1D,&Ai) != CG_OK) {
    printf("*FAILED* cgp_array_write (Array Ai)\n");
    cgp_error_exit();
  }
#endif
  t1 = MPI_Wtime();
  if(cgp_array_write_data(Ai,&min,&max,Array_i) != CG_OK) {
    printf("*FAILED* cgp_field_array_data (Array_Ai)\n");
    cgp_error_exit();
  }
  if(cgp_array_write_data(Ar,&min,&max,Array_r) != CG_OK) {
    printf("*FAILED* cgp_field_array_data (Array_r)\n");
    cgp_error_exit();
  }
  t2 = MPI_Wtime();
  xtiming[4] = t2-t1;

  if(!queue) {
    free(Array_r);
    free(Array_i);
  }

 skip1:
  
  if(cgp_close(fn) != CG_OK) {
    printf("*FAILED* cgp_close \n");
    cgp_error_exit();
  };
  /* ====================================== */
  /* ==    **  READ THE CGNS FILE **     == */
  /* ====================================== */
  MPI_Barrier(MPI_COMM_WORLD);
  /* use queued IO */
  if(cgp_queue_set(0) != CG_OK) {
    printf("*FAILED* cgp_queue_set \n");
    cgp_error_exit();
  }
  /* Open the cgns file for reading */
  if(cgp_open(fname, CG_MODE_MODIFY, &fn) != CG_OK) {
    printf("*FAILED* cgp_open \n");
    cgp_error_exit();
  }
  /* Read the base information */
  if(cg_base_read(fn, B, name, &r_cell_dim, &r_phys_dim) != CG_OK) {
    printf("*FAILED* cg_base_read\n");
    cgp_error_exit();
  }

  if(r_cell_dim != cell_dim || r_phys_dim != phys_dim) {
    printf("*FAILED* bad cell dim=%d or phy dim=%d\n", r_cell_dim, r_phys_dim);
    cgp_error_exit();
  }

  if (strcmp (name, "Base 1")) {
    printf("*FAILED* bad base name=%s\n", name);
    cgp_error_exit();
  }
  /* Read the zone information */
  if(cg_zone_read(fn, B, Z, name, sizes) != CG_OK) {
    printf("*FAILED* cg_zoneread\n");
    cgp_error_exit();
  }

  /* Check the read zone information is correct */ 
  if(sizes[0] != Nnodes) {
    printf("bad num points=%ld\n", (long)sizes[0]);
    cgp_error_exit();
  }
     
  if(sizes[1] != Nelem) {
    printf("bad num points=%ld\n", (long)sizes[1]);
    cgp_error_exit();
  }

  if(sizes[2] != 0) {
    printf("bad num points=%ld\n", (long)sizes[2]);
    cgp_error_exit();
  }

  if (strcmp (name, "Zone 1")) {
    printf("bad zone name=%s\n", name);
    cgp_error_exit();
  }
  /* ====================================== */ 
  /* ==  (A) READ THE NODAL COORDINATES  == */ 
  /* ====================================== */ 

  count = nijk[0]/comm_size;
  
  if( !(Coor_x = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_x \n");
    cgp_error_exit();
  }

  if( !(Coor_y= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_y \n");
    cgp_error_exit();
  }

  if( !(Coor_z= (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Coor_z \n");
    cgp_error_exit();
  }
  min = count*comm_rank+1;
  max = count*(comm_rank+1);

  t1 = MPI_Wtime();
  if (cgp_coord_read_data(fn,B,Z,Cx,&min,&max,Coor_x) != CG_OK) {
    printf("*FAILED* cgp_coord_read_data ( Reading Coor_x) \n");
    cgp_error_exit();
  }
  if (cgp_coord_read_data(fn,B,Z,Cy,&min,&max,Coor_y) != CG_OK) {
    printf("*FAILED* cgp_coord_read_data (Reading Coor_y) \n");
    cgp_error_exit();
  }
  if (cgp_coord_read_data(fn,B,Z,Cz,&min,&max,Coor_z) != CG_OK) {
    printf("*FAILED* cgp_coord_read_data (Reading Coor_z) \n");
    cgp_error_exit();
  }
  t2 = MPI_Wtime();
  xtiming[5] = t2-t1;
  
  /* Check if read the data back correctly */ 
  if(debug) {
    for ( k = 0; k < count; k++) {
      if( !c_double_eq(Coor_x[k], comm_rank*count + k + 1.1) ||
	  !c_double_eq(Coor_y[k], Coor_x[k] + 0.1) ||
	  !c_double_eq(Coor_z[k], Coor_y[k] + 0.1) ) {
	   printf("*FAILED* cgp_coord_read_data values are incorrect \n");
	   cgp_error_exit();
      }
    }
  }

  free(Coor_x);
  free(Coor_y);
  free(Coor_z);

/* ====================================== */ 
/* == (B) READ THE CONNECTIVITY TABLE  == */ 
/* ====================================== */ 

  count = nijk[1]/comm_size;
  if( !(elements = malloc(count*NodePerElem*sizeof(cgsize_t)) )) {
    printf("*FAILED* allocation of elements \n");
    cgp_error_exit();
  }
  
  emin = count*comm_rank+1;
  emax = count*(comm_rank+1);

  t1 = MPI_Wtime();
  if( cgp_elements_read_data(fn, B, Z, S, emin, emax, elements) != CG_OK) {
    printf("*FAILED* cgp_elements_read_data ( Reading elements) \n");
    cgp_error_exit();
  }
  t2 = MPI_Wtime();
  xtiming[6] = t2-t1;
 
  if(debug) {
    for ( k = 0; k < count; k++) {
      if(elements[k] != comm_rank*count*NodePerElem + k + 1) { 
	printf("*FAILED* cgp_elements_read_data values are incorrect\n");
	cgp_error_exit();
      }
    }
  }
  free(elements);

  /* ====================================== */ 
  /* == (C) READ THE FIELD DATA          == */ 
  /* ====================================== */
  count = nijk[0]/comm_size;

  if( !(Data_Fx = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of Reading Data_Fx \n");
    cgp_error_exit();
  }

  if( !(Data_Fy = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of  Reading Data_Fy \n");
    cgp_error_exit();
  }

  if( !(Data_Fz = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of  Reading Data_Fz \n");
    cgp_error_exit();
  }

  t1 = MPI_Wtime();

  if (cgp_field_read_data(fn,B,Z,S,Fx,&min,&max,Data_Fx) != CG_OK) {
    printf("*FAILED* cgp_field_read_data (Data_Fx) \n");
    cgp_error_exit();
  }
  if (cgp_field_read_data(fn,B,Z,S,Fy,&min,&max,Data_Fy) != CG_OK) {
    printf("*FAILED* cgp_field_read_data (Data_Fy) \n");
    cgp_error_exit();
  }
  if (cgp_field_read_data(fn,B,Z,S,Fz,&min,&max,Data_Fz) != CG_OK) {
    printf("*FAILED* cgp_field_read_data (Data_Fz) \n");
    cgp_error_exit();
  }
  t2 = MPI_Wtime();
  xtiming[7] = t2-t1;

  /* Check if read the data back correctly */ 
  if(debug) {
    for ( k = 0; k < count; k++) {
      if(!c_double_eq(Data_Fx[k], comm_rank*count + k + 1.01) ||
	 !c_double_eq(Data_Fy[k], comm_rank*count + k + 1.02) || 
	 !c_double_eq(Data_Fz[k], comm_rank*count + k + 1.03) ) {
	printf("*FAILED* cgp_field_read_data values are incorrect \n");
	cgp_error_exit();
      }
    }
  }
  free(Data_Fx);
  free(Data_Fy);
  free(Data_Fz);

  /* skipping because the memory usage is not scalable (EXAHDF-65) */
  goto skip2;
  /* ====================================== */ 
  /* == (D) READ THE ARRAY DATA          == */ 
  /* ====================================== */ 
  
  count = nijk[0]/comm_size;

  if( !(Array_r = (double*) malloc(count*sizeof(double))) ) {
    printf("*FAILED* allocation of  Reading Array_r \n");
    cgp_error_exit();
  }

  if( !(Array_i= (cgsize_t*) malloc(count*sizeof(cgsize_t))) ) {
    printf("*FAILED* allocation of  Reading Array_i  \n");
    cgp_error_exit();
  }

  min = count*comm_rank+1;
  max = count*(comm_rank+1);
  
  
  if(cg_goto(fn,B,"Zone_t",Z,"UserDefinedData_t",1,"end") != CG_OK) {
    printf("*FAILED* cg_goto (User Defined Data)\n");
    cgp_error_exit();
  }

  t1 = MPI_Wtime();
  if( cgp_array_read_data(Ar, &min, &max, Array_r) != CG_OK) {
    printf("*FAILED* cgp_field_read_data (Array_r) \n");
    cgp_error_exit();
  } 
  if( cgp_array_read_data(Ai, &min, &max, Array_i) != CG_OK) {
    printf("*FAILED* cgp_field_read_data (Array_i) \n");
    cgp_error_exit();
  } 
  t2 = MPI_Wtime();
  xtiming[8] = t2-t1;
  
  /* Check if read the data back correctly */ 
  if(debug) {
    for ( k = 0; k < count; k++) {
      if(!c_double_eq(Array_r[k], comm_rank*count + k + 1.001) ||
	 Array_i[k] != comm_rank*count*NodePerElem + k +1) {
	  printf("*FAILED* cgp_array_read_data values are incorrect \n");
	  cgp_error_exit();
      }
    }
  }

  free(Array_r);
  free(Array_i);

 skip2:

  /* t1 = MPI_Wtime(); */
  /* closeup shop and go home... */ 
  if(cgp_close(fn) !=CG_OK) {
     printf("*FAILED* cgp_close\n");
     cgp_error_exit();
  }
/*   t2 = MPI_Wtime(); */
/*   printf(" cgp_close timing = %20f \n", t2-t1); */

  xtiming[0] = t2-t0;
  
  MPI_Reduce(&xtiming, &timing, 9, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&xtiming, &timingMin, 9, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&xtiming, &timingMax, 9, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

  if(comm_rank==0) {
    sprintf(fname, "timing_%06d_%d.dat", comm_size, piomode_i+1);
    FILE *fid = fopen(fname, "w");
    if (fid == NULL) {
      printf("Error opening timing file!\n");
    } else {
      fprintf(fid,"#nprocs, wcoord, welem, wfield, warray, rcoord, relem, rfield, rarray \n");

      fprintf(fid,"%d %20f %20f %20f %20f %20f %20f  %20f %20f %20f  %20f %20f %20f  %20f %20f %20f  %20f %20f %20f  %20f %20f %20f  %20f %20f %20f  %20f %20f %20f\n", comm_size,
	      timing[0]/((double) comm_size), timingMin[0], timingMax[0],
	      timing[1]/((double) comm_size), timingMin[1], timingMax[1],
	      timing[2]/((double) comm_size), timingMin[2], timingMax[2],
	      timing[3]/((double) comm_size), timingMin[3], timingMax[3],
	      timing[4]/((double) comm_size), timingMin[4], timingMax[4],
	      timing[5]/((double) comm_size), timingMin[5], timingMax[5],
	      timing[6]/((double) comm_size), timingMin[6], timingMax[6],
	      timing[7]/((double) comm_size), timingMin[7], timingMax[7],
	      timing[8]/((double) comm_size), timingMin[8], timingMax[8] );
    }
  }

  MPI_Finalize();

  return 0;
}

        
