#include "atelem.c"
#include "atlalib.c"
#include "driftkickrad.c"  /* fastdrift.c, strthinkick.c */
#include "exactdrift.c"
#include "exactmultipolefringe.c"

#define DRIFT1 0.6756035959798286638
#define DRIFT2 -0.1756035959798286639
#define KICK1 1.351207191959657328
#define KICK2 -1.702414383919314656

struct elem {
  double Length;
  double *PolynomA;
  double *PolynomB;
  int MaxOrder;
  int NumIntSteps;
  double Energy;
  /* Optional fields */
  int multipole_fringe;
  double *R1;
  double *R2;
  double *T1;
  double *T2;
  double *RApertures;
  double *EApertures;
  double *KickAngle;
};

static void multipole_pass(
  double *r, double le, double *A, double *B, int max_order, int num_int_steps,
  int do_fringe,
  double *T1, double *T2, double *R1, double *R2, double *RApertures,
  double *EApertures, double *KickAngle, double E0, int num_particles)
{
  int c;
  double SL = le / num_int_steps;
  double L1 = SL * DRIFT1;
  double L2 = SL * DRIFT2;
  double K1 = SL * KICK1;
  double K2 = SL * KICK2;
  double B0 = B[0];
  double A0 = A[0];

  if (KickAngle) { /* Convert corrector component to polynomial coefficients */
    B[0] -= sin(KickAngle[0]) / le;
    A[0] += sin(KickAngle[1]) / le;
  }
  #pragma omp parallel for if (num_particles > OMP_PARTICLE_THRESHOLD) \
                       default(none) \
                       shared(r, num_particles, R1, T1, R2, T2, RApertures, \
                       EApertures, A, B, L1, L2, K1, K2, max_order, \
                       num_int_steps, FringeQuadEntrance, useLinFrEleEntrance, \
                       FringeQuadExit, useLinFrEleExit, fringeIntM0, fringeIntP0) \
                       private(c)
  for (c = 0; c < num_particles; c++) { /*Loop over particles  */
    double *r6 = r + c * 6;
    if (!atIsNaN(r6[0])) {
      int m;

      /*  misalignment at entrance  */
      if (T1) ATaddvv(r6, T1);
      if (R1) ATmultmv(r6, R1);

      /* Check physical apertures at the entrance of the magnet */
      if (RApertures) checkiflostRectangularAp(r6, RApertures);
      if (EApertures) checkiflostEllipticalAp(r6, EApertures);

      /*  integrator  */
      if (do_fringe)
        multipole_fringe(r6, le, A, B, max_order, 1.0, 0);
      for (m = 0; m < num_int_steps; m++) { /*  Loop over slices */
        exact_drift(r6, L1);
        strthinkickrad(r6, A, B, K1, E0, max_order);
        exact_drift(r6, L2);
        strthinkickrad(r6, A, B, K2, E0, max_order);
        exact_drift(r6, L2);
        strthinkickrad(r6, A, B, K1, E0, max_order);
        exact_drift(r6, L1);
      }
      if (do_fringe)
        multipole_fringe(r6, le, A, B, max_order, -1.0, 0);

      /* Check physical apertures at the exit of the magnet */
      if (RApertures) checkiflostRectangularAp(r6, RApertures);
      if (EApertures) checkiflostEllipticalAp(r6, EApertures);

      /* Misalignment at exit */
      if (R2) ATmultmv(r6, R2);
      if (T2) ATaddvv(r6, T2);
    }
  }
  /* Remove corrector component in polynomial coefficients */
  B[0] = B0;
  A[0] = A0;
}

#if defined(MATLAB_MEX_FILE) || defined(PYAT)
ExportMode struct elem *trackFunction(const atElem *ElemData, struct elem *Elem,
                                      double *r_in, int num_particles,
                                      struct parameters *Param) {
  if (!Elem) {
    double Length = atGetDouble(ElemData, "Length"); check_error();
    double *PolynomA = atGetDoubleArray(ElemData, "PolynomA"); check_error();
    double *PolynomB = atGetDoubleArray(ElemData, "PolynomB"); check_error();
    int MaxOrder = atGetLong(ElemData, "MaxOrder"); check_error();
    int NumIntSteps = atGetLong(ElemData, "NumIntSteps"); check_error();
    double Energy=atGetDouble(ElemData,"Energy"); check_error();
    /*optional fields*/
    int multipole_fringe = atGetOptionalLong(ElemData, "MultipoleFringe", 0); check_error();
    double *R1 = atGetOptionalDoubleArray(ElemData, "R1"); check_error();
    double *R2 = atGetOptionalDoubleArray(ElemData, "R2"); check_error();
    double *T1 = atGetOptionalDoubleArray(ElemData, "T1"); check_error();
    double *T2 = atGetOptionalDoubleArray(ElemData, "T2"); check_error();
    double *EApertures = atGetOptionalDoubleArray(ElemData, "EApertures"); check_error();
    double *RApertures = atGetOptionalDoubleArray(ElemData, "RApertures"); check_error();
    double *KickAngle = atGetOptionalDoubleArray(ElemData, "KickAngle"); check_error();

    Elem = (struct elem *)atMalloc(sizeof(struct elem));
    Elem->Length = Length;
    Elem->PolynomA = PolynomA;
    Elem->PolynomB = PolynomB;
    Elem->MaxOrder = MaxOrder;
    Elem->NumIntSteps = NumIntSteps;
    Elem->Energy=Energy;
    /*optional fields*/
    Elem->multipole_fringe = multipole_fringe;
    Elem->R1 = R1;
    Elem->R2 = R2;
    Elem->T1 = T1;
    Elem->T2 = T2;
    Elem->EApertures = EApertures;
    Elem->RApertures = RApertures;
    Elem->KickAngle = KickAngle;
  }
  multipole_pass(r_in, Elem->Length, Elem->PolynomA, Elem->PolynomB,
                 Elem->MaxOrder, Elem->NumIntSteps,
                 Elem->multipole_fringe,
                 Elem->T1, Elem->T2, Elem->R1, Elem->R2,
                 Elem->RApertures, Elem->EApertures,
                 Elem->KickAngle, Elem->Energy, num_particles);
  return Elem;
}

MODULE_DEF(ExactMultipoleRadPass) /* Dummy module initialisation */

#endif /*defined(MATLAB_MEX_FILE) || defined(PYAT)*/

#if defined(MATLAB_MEX_FILE)
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
  if (nrhs == 2) {
    double *r_in;
    const mxArray *ElemData = prhs[0];
    int num_particles = mxGetN(prhs[1]);

    double Length = atGetDouble(ElemData, "Length"); check_error();
    double *PolynomA = atGetDoubleArray(ElemData, "PolynomA"); check_error();
    double *PolynomB = atGetDoubleArray(ElemData, "PolynomB"); check_error();
    int MaxOrder = atGetLong(ElemData, "MaxOrder"); check_error();
    int NumIntSteps = atGetLong(ElemData, "NumIntSteps"); check_error();
    double Energy=atGetDouble(ElemData,"Energy"); check_error();
    /*optional fields*/
    int multipole_fringe = atGetOptionalLong(ElemData, "MultipoleFringe", 0); check_error();
    double *R1 = atGetOptionalDoubleArray(ElemData, "R1"); check_error();
    double *R2 = atGetOptionalDoubleArray(ElemData, "R2"); check_error();
    double *T1 = atGetOptionalDoubleArray(ElemData, "T1"); check_error();
    double *T2 = atGetOptionalDoubleArray(ElemData, "T2"); check_error();
    double *EApertures = atGetOptionalDoubleArray(ElemData, "EApertures"); check_error();
    double *RApertures = atGetOptionalDoubleArray(ElemData, "RApertures"); check_error();
    double *KickAngle = atGetOptionalDoubleArray(ElemData, "KickAngle"); check_error();

    /* ALLOCATE memory for the output array of the same size as the input  */
    plhs[0] = mxDuplicateArray(prhs[1]);
    r_in = mxGetDoubles(plhs[0]);
    multipole_pass(r_in, Length, PolynomA, PolynomB, MaxOrder, NumIntSteps,
                   multipole_fringe,
                   T1, T2, R1, R2,
                   RApertures, EApertures,
                   KickAngle, Energy, num_particles);
  } else if (nrhs == 0) {
    /* list of required fields */
    int i0 = 0;
    plhs[0] = mxCreateCellMatrix(6, 1);
    mxSetCell(plhs[0], i0++, mxCreateString("Length"));
    mxSetCell(plhs[0], i0++, mxCreateString("PolynomA"));
    mxSetCell(plhs[0], i0++, mxCreateString("PolynomB"));
    mxSetCell(plhs[0], i0++, mxCreateString("MaxOrder"));
    mxSetCell(plhs[0], i0++, mxCreateString("NumIntSteps"));
    mxSetCell(plhs[0], i0++, mxCreateString("Energy"));
    if (nlhs > 1) {
      /* list of optional fields */
      int i1 = 0;
      plhs[1] = mxCreateCellMatrix(8, 1);
      mxSetCell(plhs[1], i1++, mxCreateString("MultipoleFringe"));
      mxSetCell(plhs[1], i1++, mxCreateString("T1"));
      mxSetCell(plhs[1], i1++, mxCreateString("T2"));
      mxSetCell(plhs[1], i1++, mxCreateString("R1"));
      mxSetCell(plhs[1], i1++, mxCreateString("R2"));
      mxSetCell(plhs[1], i1++, mxCreateString("RApertures"));
      mxSetCell(plhs[1], i1++, mxCreateString("EApertures"));
      mxSetCell(plhs[1], i1++, mxCreateString("KickAngle"));
    }
  } else {
    mexErrMsgIdAndTxt("AT:WrongArg", "Needs 0 or 2 arguments");
  }
}
#endif /*MATLAB_MEX_FILE*/
