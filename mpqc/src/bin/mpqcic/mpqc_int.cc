
/* mpqc_int.cc -- functions to allow use of libGeom with old mpqc code
 *
 *      THIS SOFTWARE FITS THE DESCRIPTION IN THE U.S. COPYRIGHT ACT OF A
 *      "UNITED STATES GOVERNMENT WORK".  IT WAS WRITTEN AS A PART OF THE
 *      AUTHOR'S OFFICIAL DUTIES AS A GOVERNMENT EMPLOYEE.  THIS MEANS IT
 *      CANNOT BE COPYRIGHTED.  THIS SOFTWARE IS FREELY AVAILABLE TO THE
 *      PUBLIC FOR USE WITHOUT A COPYRIGHT NOTICE, AND THERE ARE NO
 *      RESTRICTIONS ON ITS USE, NOW OR SUBSEQUENTLY.
 *
 *  Author:
 *      E. T. Seidl
 *      Bldg. 12A, Rm. 2033
 *      Computer Systems Laboratory
 *      Division of Computer Research and Technology
 *      National Institutes of Health
 *      Bethesda, Maryland 20892
 *      Internet: seidl@alw.nih.gov
 *      March, 1993
 */

#include <util/class/class.h>
#include <util/state/state.h>
#include <util/keyval/keyval.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/molecule/simple.h>
#include <chemistry/molecule/symm.h>
#include <chemistry/molecule/simpleQCList.h>
#include <chemistry/molecule/symmQCList.h>
#include <math/nihmatrix/nihmatrix.h>
#include <math/nihmatrix/lmath.h>

#include <math.h>

#include <chemistry/qc/geom/retcodes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mpqc_int.h"

#define STATEOUT StateOutBinXDR
#define STATEIN StateInBinXDR
//#define STATEOUT StateOutBin
//#define STATEIN StateInBin
//#define STATEOUT StateOutText
//#define STATEIN StateInText

/////////////////////////////////////////////////////////////////

static FILE *outfp=stdout;
static FILE *errfp=stderr;

static centers_t *centers;
static Molecule mol;

static RefSymmCoList symm_coords;

enum updates { NONE, BFGS, DFP, POWELL, MURT, BERNY };
enum methods { POLACK, FLETCH, NEWTR, GDIIS, EFC };

static int iter=1;
struct diis_stuff {
  int nsave;
  DVector *coords;
  DVector *grads;
  DVector *error;
  } save_diis = { 0, 0, 0 };

static double gdiis_begin=1.0e-2;
static int ngdiis=5;
static int gdiis=1;
static int bfgs=1;
static int redundant=0;
static int justa1=1;
static int update_hess=1;
static int recalc_hess=0;
static int use_gdiis=1;
static double conv_crit=1.0e-6;
static int maxstep=50;
static double maxstepsize=0.3;
static int recalc_bmat=0;
static int cartesians=0;

static double maxforce=0;
static double rmsforce=0;
static double maxdisp=0;
static double rmsdisp=0;

static DMatrix hessian,oldhessian;
static DVector iforce,oldiforce;
static DVector intco,oldintco;
static DVector idisp,oldidisp;

static int internal_forces(DMatrix&,Molecule&,double_matrix_t*);
static int update_hessian(RefKeyVal);
static int cartesian_disp(DMatrix&);
static void gdiis_disp();
static void guess_hessian(RefSimpleCoList,RefKeyVal);
static int get_symmco(RefKeyVal);
static DMatrix gen_inverse(DMatrix&);
static void get_input(RefKeyVal);
static int setup_cart(centers_t *cs,RefKeyVal);

/////////////////////////////////////////////////////////////////

int
Geom_init_mpqc(FILE *out, FILE *err, centers_t *cs, RefKeyVal keyval)
{
  int i;

 // we don't want to get into the business of providing a centers struct
 // let the user do that
  if(!cs) {
    err_msg("Geom_init_mpqc: centers is null");
    return GEOM_ABORT;
    }

  centers=cs;

 // set up the default output and error files
  if(out) outfp=out;
  if(err) errfp=err;

  get_input(keyval);

  if (cartesians) return setup_cart(cs,keyval);

 // if geom.dat exists, then read in all of the hooey stored there,
 // otherwise read intco stuff from mpqc.in

  struct stat buf;

  if(stat("geom.dat",&buf) < 0 || buf.st_size==0) {
    STATEOUT so("geom.dat","w+");

   // ok, first let's create the molecule object using the coords in centers

    for (i=0; i < centers->n; i++) {
      char* label = keyval->pcharvalue("atom_labels",i);
      AtomicCenter ac(centers->center[i].atom,
		      centers->center[i].r[0],
		      centers->center[i].r[1],
		      centers->center[i].r[2],
                      label
                      );
      delete[] label;
      mol.add_atom(i,ac);
      }

   // now that we have a geometry, let's make the list of symm coordinates
    if(get_symmco(keyval) < 0) {
      err_msg("Geom_init_mpqc: yikes!  can't make symm coords");
      return GEOM_ABORT;
      }

    int nsym=0;
    for(SymmCoListIter scp=symm_coords; scp; scp++,nsym++) ;

    intco.resize(nsym);
    intco.zero();

    iforce.resize(nsym);
    iforce.zero();

    idisp.resize(nsym);
    idisp.zero();

    for(scp=0,i=0; scp; scp++,i++) {
      intco[i] = scp->value();
      }

  // save it all to disk
    so.put(iter);
    symm_coords->save_state(so);
    mol.save_object_state(so);
    intco.save_object_state(so);
    iforce.save_object_state(so);
    idisp.save_object_state(so);
    hessian.save_object_state(so);
    }
  else {
    STATEIN si("geom.dat","r+");

    si.get(iter);

    fprintf(outfp,"\n restarting geometry optimization at iteration %d\n",iter);

    symm_coords = SymmCoList::restore_state(si);
    Molecule mol2(si); mol=mol2;
    DVector ic(si);    intco=ic;
    DVector ifc(si);    iforce=ifc;
    DVector id(si);    idisp=id;
    DMatrix h(si);     hessian=h;

    for(i=0; i < centers->n; i++) {
      centers->center[i].r[0] = mol[i][0];
      centers->center[i].r[1] = mol[i][1];
      centers->center[i].r[2] = mol[i][2];
      }
    }

  fprintf(outfp,"\n initial geometry in Geom_init_mpqc\n");
  mol.print(outfp);

  //fprintf(outfp,"\n internal coordinates in Geom_init_mpqc\n");
  //Geom_print_pretty(symm_coords);

  return GEOM_COMPUTE_GRADIENT;
  }

void
Geom_done_mpqc(RefKeyVal keyval)
{
  symm_coords = 0;

  RefSimpleCoList list = Geom_form_simples(mol);

  int nadd;
  if(nadd=keyval->count("add_simp")) {
    for(int i=0; i < nadd; i++) {
      char *val = keyval->pcharvalue("add_simp",i,0);

      if (!strcmp("stre",val))
        list->add(new Stre(keyval.pointer(),"add_simp",i));
      else if (!strcmp("bend",val))
        list->add(new Bend(keyval.pointer(),"add_simp",i));
      else if (!strcmp("tors",val))
        list->add(new Tors(keyval.pointer(),"add_simp",i));
      else if (!strcmp("out",val))
        list->add(new Out(keyval.pointer(),"add_simp",i));
      else if (!strcmp("linip",val))
        list->add(new LinIP(keyval.pointer(),"add_simp",i));
      else if (!strcmp("linop",val))
        list->add(new LinOP(keyval.pointer(),"add_simp",i));
      delete[] val;
      }
    }

  Geom_calc_simples(list,mol);

  fprintf(outfp,"\n  Final internal coordinates\n");
  Geom_print_pretty(list);
}

static void
get_input(RefKeyVal keyval)
{
  if (keyval->exists("convergence")) {
    int conv = keyval->intvalue("convergence");
    conv_crit = pow(10.0,(double)-conv);
    }

  if (keyval->exists("maxstepsize")) {
    maxstepsize = keyval->doublevalue("maxstepsize");
    }

  if (keyval->exists("cartesians")) {
    cartesians = keyval->booleanvalue("cartesians");
    }

  if (keyval->exists("update_hessian")) {
    update_hess = keyval->booleanvalue("update_hessian");
    }

  if (keyval->exists("maxstep")) {
    maxstep = keyval->intvalue("maxstep");
    }

  if (keyval->exists("recalc_bmat")) {
    recalc_bmat = keyval->booleanvalue("recalc_bmat");
    }

  if (keyval->exists("recalc_hessian")) {
    recalc_hess = keyval->booleanvalue("recalc_hessian");
    }

  if (keyval->exists("ngdiis")) {
    ngdiis = keyval->intvalue("ngdiis");
    }

  if (keyval->exists("use_gdiis")) {
    use_gdiis = keyval->booleanvalue("use_gdiis");
    }

  if (keyval->exists("gdiis_begin")) {
    int tmp = keyval->intvalue("gdiis_begin");
    gdiis_begin = pow(10.0,(double)-tmp);
    }

  redundant = keyval->booleanvalue("redundant");
  if (redundant > 0) justa1=0;

  fprintf(outfp,"\n  intco:use_gdiis      = %d\n",use_gdiis);
  fprintf(outfp,"  intco:redundant      = %d\n",redundant);
  fprintf(outfp,"  intco:cartesians     = %d\n",cartesians);
  fprintf(outfp,"  intco:update_hessian = %d\n",update_hess);
  fprintf(outfp,"  intco:recalc_hessian = %d\n",recalc_hess);
  fprintf(outfp,"  intco:recalc_bmat    = %d\n",recalc_bmat);
  fprintf(outfp,"  intco:ngdiis         = %d\n",ngdiis);
  fprintf(outfp,"  intco:maxstep        = %d\n",maxstep);
  fprintf(outfp,"  intco:maxstepsize    = %g\n",maxstepsize);
  fprintf(outfp,"  intco:convergence    = %g\n",conv_crit);
  fprintf(outfp,"  intco:gdiis_begin    = %g\n\n",gdiis_begin);
}

static int
get_symmco(RefKeyVal keyval)
{
  static int firsttime=1;

  RefSimpleCoList list;

 // if the simples are defined in the input, read them, otherwise generate
 // them based on the geometry

  if(keyval->count("simp"))
    list = Geom_read_simples(keyval.pointer());
  else
    list = Geom_form_simples(mol);

  int nadd;
  if(nadd=keyval->count("add_simp")) {
    for(int i=0; i < nadd; i++) {
      char *val = keyval->pcharvalue("add_simp",i,0);

      if (!strcmp("stre",val))
        list->add(new Stre(keyval.pointer(),"add_simp",i));
      else if (!strcmp("bend",val))
        list->add(new Bend(keyval.pointer(),"add_simp",i));
      else if (!strcmp("tors",val))
        list->add(new Tors(keyval.pointer(),"add_simp",i));
      else if (!strcmp("out",val))
        list->add(new Out(keyval.pointer(),"add_simp",i));
      else if (!strcmp("linip",val))
        list->add(new LinIP(keyval.pointer(),"add_simp",i));
      else if (!strcmp("linop",val))
        list->add(new LinOP(keyval.pointer(),"add_simp",i));
      delete[] val;
      }
    }

  if(keyval->count("symm")) {
    symm_coords = Geom_read_symm(keyval.pointer(),"symm",list);
    if(!redundant) symm_coords = Geom_form_symm(mol,symm_coords,justa1);
    }
  else if(redundant)
    symm_coords = Geom_symm_from_simple(list,1);
  else
    symm_coords = Geom_form_symm(mol,list,justa1);

  if(!symm_coords) {
    err_msg("get_symmco: yikes!  no symmetrized internal coords");
    return -1;
    }

  int csize;
  if ((csize=keyval->count("chessian"))) {
    csize = (int) sqrt((double)csize);
    DMatrix chessian(csize,csize);
    int ij=0;
    for (int i=0; i < csize; i++)
      for (int j=0; j < csize; j++,ij++)
        chessian(i,j) = keyval->doublevalue("chessian",ij);

    chessian *= 4.359813653/(0.52917706*0.52917706);

    DMatrix bmat = Geom_make_bmat(symm_coords,mol);
    DMatrix bmbt = gen_inverse(bmat * bmat.transpose());

    hessian = bmbt * bmat * chessian * bmat.transpose() * bmbt;
    for (i=0; i < hessian.nrow(); i++) {
      for (int j=0; j < i; j++) {
        hessian(i,j) += hessian(j,i);
        hessian(j,i) = hessian(i,j) = 0.5*hessian(i,j);
        }
      }

    bmbt = bmat*bmat.transpose();
    DMatrix p = bmbt * gen_inverse(bmbt);
    hessian = p * gen_inverse(p*hessian*p) * p;
    }
  else
    guess_hessian(list,keyval);

  if(list) {
    if(firsttime) {
      fprintf(outfp,"  Simple Internal Coordinates\n");
      Geom_calc_simples(list,mol);
      Geom_print_pretty(list);
      firsttime=0;
      }
    }

  fprintf(outfp,"\n  in get_symmco:  %d symmetrized internal coordinates\n",
                hessian.nrow());

  return 0;
  }

int
Geom_update_mpqc(double energy, double_matrix_t *grad, double_matrix_t *hess,
                 RefKeyVal keyval)
{

  oldiforce = iforce;
  oldintco = intco;
  oldidisp = idisp;
  oldhessian = hessian;
    
  int i;
  DMatrix bmat;

  // find rms and max force
  rmsforce=0;
  maxforce=0;
  for (i=0; i < grad->n1; i++) {
    for (int j=0; j < grad->n2; j++) {
      double d = fabs(grad->d[i][j]);
      rmsforce += d*d;
      maxforce = (d>maxforce) ? d : maxforce;
      }
    }
  rmsforce = sqrt(rmsforce/(grad->n1*grad->n2));
      
  if (!cartesians) {

   // calculate new internal coordinates and the b matrix
    bmat = Geom_make_bmat(symm_coords,mol);

   // and now the new internal forces
    internal_forces(bmat,mol,grad);

    fprintf(outfp,"\n internal coordinates and forces\n");

    i=0;
    for(SymmCoListIter scp=symm_coords; scp; scp++,i++) {
      intco[i] = scp->value();
      fprintf(outfp," %5d %15.10f %15.10f\n",i+1,intco[i],iforce[i]);
      }
    }
  else {
    for (int i=0; i < iforce.dim(); i++) {
      iforce[i] = grad->d[i%3][i/3]*8.2388575;
      intco[i] = mol[i/3][i%3] * 0.52917706;
      }
    }

  if (update_hess)
    update_hessian(keyval);

  if (use_gdiis) {
    gdiis_disp();
    }
  else { // the NR step
    idisp = -1*(hessian * iforce);
    }

 // scale the displacement vector if too large
  {
     double tot=0,scal=1.0;
     tot = sqrt(idisp.dot(idisp));
     //for(i=0; i < idisp.dim(); i++) tot += fabs(idisp[i]);
     if(tot>maxstepsize) {
       scal=maxstepsize/tot;
       fprintf(outfp,"\n stepsize of %f is too big, scaling by %f\n",tot,scal);
       }
     fprintf(outfp,"\n taking step of size %f\n",tot*scal);
     for(i=0; i < idisp.dim(); i++) idisp[i] *= scal;
     }

  if (cartesians) {
    rmsdisp=0;
    for (i=0; i < idisp.dim(); i++) {
      mol[i/3][i%3] += idisp[i] / 0.52917706;
      rmsdisp += (idisp[i] / 0.52917706)*(idisp[i] / 0.52917706);
      }
    
    rmsdisp = sqrt(rmsdisp/idisp.dim());
    maxdisp = idisp.maxval()/0.52917706;
    }
      
  if (!cartesians && cartesian_disp(bmat) < 0) {
    fprintf(outfp,"\n could not update geometry, saving state and exiting\n");
    STATEOUT so("geom.dat");
    so.put(iter);
    symm_coords->save_state(so);
    mol.save_object_state(so);
    intco.save_object_state(so);
    iforce.save_object_state(so);
    idisp.save_object_state(so);
    hessian.save_object_state(so);
    return GEOM_ABORT;
    }
  else {
    fprintf(outfp,"\n displacements and new internal coordinates\n");
    for(i=0; i < intco.dim(); i++)
      fprintf(outfp," %5d %15.10f %15.10f\n",i+1,idisp[i],intco[i]);

    fprintf(outfp,"\n max of 1/2 idisp*iforce = %15.10g\n",
			      (idisp * iforce).maxval()/2);
    fprintf(outfp," max force               = %15.10g\n",maxforce);
    fprintf(outfp," rms force               = %15.10g\n",rmsforce);
    fprintf(outfp," max disp                = %15.10g\n",maxdisp);
    fprintf(outfp," rms disp                = %15.10g\n",rmsdisp);

    fprintf(outfp,"\n updated geometry\n");
    mol.print(outfp);
    }

  STATEOUT so("geom.dat");

  iter++;

  so.put(iter);
  symm_coords->save_state(so);
  mol.save_object_state(so);
  intco.save_object_state(so);
  iforce.save_object_state(so);
  idisp.save_object_state(so);
  hessian.save_object_state(so);

  for(i=0; i < centers->n; i++) {
    centers->center[i].r[0] = mol[i][0];
    centers->center[i].r[1] = mol[i][1];
    centers->center[i].r[2] = mol[i][2];
    }

  if (((idisp * iforce).maxval()/2) < conv_crit)
    return GEOM_DONE;
  else
    return GEOM_COMPUTE_GRADIENT;
  }


static int
internal_forces(DMatrix& bmat, Molecule& m, double_matrix_t *grad)
{
  int i,j,k;
  int nsym=bmat.nrow();

  DMatrix gminus = gen_inverse(bmat * bmat.transpose());

  DVector gradient(grad->n1*grad->n2);
  double sum=0;

  for(i=0; i < grad->n2; i++) {
    gradient[3*i] =   grad->d[0][i];
    gradient[3*i+1] = grad->d[1][i];
    gradient[3*i+2] = grad->d[2][i];
    }

 // convert gradient from au/bohr to mdyne
  gradient *= 8.2388575;

  iforce = gminus*bmat*gradient;

  return 0;
  }

static int
update_hessian(RefKeyVal keyval)
{
  if(iter==1) return 0;

  if (!recalc_hess) {
    DVector delta = iforce - oldiforce;
    DVector v = oldhessian * delta;
    double alpha = 1.0/oldidisp.dot(delta);
    double beta = (1.0 + alpha*delta.dot(v))*alpha;

    hessian = oldhessian + beta*(oldidisp.ccross(oldidisp)) -
             alpha*(oldidisp.ccross(v) + v.ccross(oldidisp));
    }
  else {
    RefSimpleCoList list = Geom_form_simples(mol);
    Geom_calc_simples(list,mol);
    guess_hessian(list,keyval);
    fprintf(outfp," recalc hessian\n");
    }

  //hessian->inverse().print("\n improved hessian");

  return 0;
  }

static int
cartesian_disp(DMatrix& bmat)
{
  int i;
  int ntry=0;
  int step;
  int nsym=intco.dim();
  int n3 = bmat.ncol();
  SymmCoListIter p;

  DMatrix gminus,nbmat;

  if (!recalc_bmat) {
    gminus = gen_inverse(bmat*bmat.transpose());
    nbmat = bmat;
    }

  Molecule newg;
  DVector ndisp(nsym);
  DVector tintco(nsym);
  DVector tv1(nsym);

Foo:
  tintco = intco + idisp;
  ndisp= idisp;
  newg = mol;
  step=0;

  do {
    if (recalc_bmat) {
      nbmat = Geom_make_bmat(symm_coords,newg);
      gminus = gen_inverse(nbmat*nbmat.transpose());
      }

    DVector tv2 = gminus*ndisp;

    for(i=0; i < n3; i++) {
      double t=0;
      for(int j=0; j < nsym; j++)
        t += nbmat[j][i]*tv2[j];

      newg[i/3][i%3] += t*1.88972666;
      }

    for(p=symm_coords,i=0; p; p++,i++) {
      p->calc_intco(newg);
      tv1[i] = p->value();
      }

    if(tv2.maxval() < 1.0e-10) break;

    step++;

    ndisp = tintco - tv1;
    } while(step < maxstep);

  if(step==maxstep && ntry < 4) {
    fprintf(outfp," too many steps, scaling displacements by 0.5\n");
    idisp *= 0.5;
    ntry++;
    goto Foo;
    }
  else if (step==maxstep) {
    fprintf(outfp," just would not converge\n");
    idisp *= 16.0;
    return -1;
    }

  fprintf(outfp," converged in %d steps\n",step);

 // figure out max and rms cartesian displacements
  maxdisp=0;
  rmsdisp=0;
  for (i=0; i < mol.natom(); i++) {
    double dx = fabs(mol[i][0]-newg[i][0]);
    double dy = fabs(mol[i][1]-newg[i][1]);
    double dz = fabs(mol[i][2]-newg[i][2]);

    rmsdisp += dx*dx + dy*dy + dz*dz;

   // exercises a gcc 2.4.5 compiler bug
   // maxdisp = (dx > maxdisp) ? dx : maxdisp;
   // maxdisp = (dy > maxdisp) ? dy : maxdisp;
   // maxdisp = (dz > maxdisp) ? dz : maxdisp;

    if (dx > maxdisp) maxdisp = dx;
    if (dy > maxdisp) maxdisp = dy;
    if (dz > maxdisp) maxdisp = dz;
    }
  rmsdisp = sqrt(rmsdisp/(mol.natom()*3));

  mol=newg;

  idisp = tv1 - intco;
  intco = tv1;

  return 0;
  }

// hack number one, let's see if we can get this to work

static int diis_iter=0;

static void
gdiis_disp()
{
  int i,j,ii,jj;

  if (rmsforce > gdiis_begin) {
    idisp = -1*(hessian * iforce);
    return;
    }

  diis_iter++;

  int howmany = (diis_iter < ngdiis) ? diis_iter : ngdiis;

  if(diis_iter==1) {
    save_diis.nsave=ngdiis;
    save_diis.coords = new DVector[ngdiis];
    save_diis.grads = new DVector[ngdiis];
    save_diis.error = new DVector[ngdiis];
    }

 // save the current gradient and coordinates
  if(diis_iter <= ngdiis) {
    save_diis.coords[diis_iter-1] = intco;
    save_diis.grads[diis_iter-1] = iforce;
    }
  else {
    for(i=0; i < ngdiis-1; i++) {
      save_diis.coords[i] = save_diis.coords[i+1];
      save_diis.grads[i] = save_diis.grads[i+1];
      }
    save_diis.coords[ngdiis-1] = intco;
    save_diis.grads[ngdiis-1] = iforce;
    }

  if (diis_iter==1) {
    idisp = -1*(hessian * iforce);
    return;
    }

 // now let's form the error matrices
  for(i=0; i < howmany; i++)
    save_diis.error[i] = -1*(hessian * save_diis.grads[i]); 

 // and now the A matrix

  DMatrix A;
  DVector coeff;
  double determ;
  int ntry=0;

  do {
    int num=howmany-ntry;

    A.resize(num+1,num+1);
    coeff.resize(num+1);

    for(ii=0,i=ntry; i < howmany; i++,ii++) {
      coeff[ii]=0;
      for(j=ntry,jj=0; j <= i; j++,jj++)
	A(ii,jj) = A(jj,ii) = save_diis.error[i].dot(save_diis.error[j]);
      }
    A *= 1/A(0,0);

    coeff[num]=1;
    for(i=0; i < num; i++)
      A(num,i) = A(i,num) = 1;
    A(num,num) = 0;
    
    ntry++;
    } while((determ=fabs(A.solve_lin(coeff))) < 1.0e-12);

  DVector delstar(intco.dim());
  DVector xstar(intco.dim());

  delstar.zero();
  xstar.zero();

  for(i=0,ii=ntry-1; ii < howmany; i++,ii++) {
    delstar += save_diis.grads[ii]*coeff[i];
    xstar += save_diis.coords[ii]*coeff[i];
    }

   idisp = xstar - intco - hessian*delstar;
  }

static void
guess_hessian(RefSimpleCoList list,RefKeyVal keyval)
{

 // see if there is a hessian in the input
  int size= keyval->count("hessian");

  if(size==0 || size != hessian.nrow()*(hessian.nrow()+1)/2) {
    hessian = Geom_form_hessian(mol,list,symm_coords);
    }
  else {
    int i,j,ij;
    for(i=ij=0; i < hessian.nrow(); i++)
      for(j=0; j <= i; j++,ij++)
        hessian(i,j) = hessian(j,i) = keyval->doublevalue("hessian",ij);
    }

  DMatrix bmat = Geom_make_bmat(symm_coords,mol);
  DMatrix bmbt = bmat*bmat.transpose();
  DMatrix p = bmbt * gen_inverse(bmbt);
  hessian = p * gen_inverse(p*hessian*p) * p;

  }

static DMatrix
gen_inverse(DMatrix& mat)
{
  DMatrix vecs(mat.nrow(),mat.nrow());
  DVector vals(mat.nrow());

  mat.diagonalize(vals,vecs);

  DMatrix lam(mat.nrow(),mat.nrow());
  lam.zero();

  for(int i=0; i < lam.nrow(); i++)
    if(vals[i] > 1.0e-8) lam(i,i) = 1.0/vals[i];

  lam = vecs * lam * vecs.transpose();

  return lam;
  }


static int
setup_cart(centers_t *cs,RefKeyVal keyval)
{
  int i,j;

 // ok, first let's create the molecule object using the coords in centers

  for (i=0; i < centers->n; i++) {
    char* label = keyval->pcharvalue("atom_labels",i);
    AtomicCenter ac(centers->center[i].atom,
                    centers->center[i].r[0],
                    centers->center[i].r[1],
                    centers->center[i].r[2],
                    label
                    );
    delete[] label;
    mol.add_atom(i,ac);
    }

 // form simple diagonal hessian, and transform to cartesian coords
  RefSimpleCoList  list = Geom_form_simples(mol);

  DMatrix ihessian = Geom_form_hessian(mol,list);
  RefSymmCoList slist = Geom_symm_from_simple(list);
  DMatrix bmat = Geom_make_bmat(slist,mol);

  DMatrix thessian = bmat.transpose() * ihessian * bmat;

 // now remove the 3 translations and 3 rotations and invert the hessian
  int dim=thessian.nrow();
  DMatrix evecs(dim,dim);
  DVector evals(dim);

  thessian.diagonalize(evals,evecs);

  hessian.resize(dim,dim);
  hessian.zero();
  
  int ii,jj,kk;
  i=0;
  for (ii=0; ii < dim; ii++) {
    if (evals[ii] < 1e-8 && evals[ii] > -1e-8) {
      for (kk=0; kk < dim; kk++) evecs(kk,ii)=0;
      }
    else {
      double sum=0;
      for (kk=0; kk < dim; kk++) sum += mol[kk/3][kk%3]*evecs(kk,ii);
      if (sum < 1e-8 && sum > -1e-8) {
        for (kk=0; kk < dim; kk++) evecs(kk,ii)=0;
        }
      else
        i++;
      }
    }

  fprintf(outfp,
    "\n %d totally symmetric eigenvectors in cartesian hessian\n",i);

  for (ii=0; ii < dim; ii++) {
    for (jj=0; jj < dim; jj++) {
      for (kk=0; kk < dim; kk++) {
        if (evals[kk] > 1e-10 || evals[kk] < -1e-10)
          hessian(ii,jj) += evecs(ii,kk) * evecs(jj,kk) / evals[kk];
        }
      }
    }

  intco.resize(dim);
  intco.zero();

  iforce.resize(dim);
  iforce.zero();

  idisp.resize(dim);
  idisp.zero();

  return GEOM_COMPUTE_GRADIENT;
  }
