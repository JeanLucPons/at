#ifdef PYAT

#include <Python.h>
#include "AbstractGPU.h"
#include "PyInterface.h"
#include "iostream"

using namespace std;

// Particle type
PyObject *particle_type;

// Methods declaration
static PyObject *at_gpuinfo(PyObject *self, PyObject *args, PyObject *kwargs);
static PyObject *at_gpupass(PyObject *self, PyObject *args, PyObject *kwargs);

// Method table
static PyMethodDef AtGPUMethods[] = {
        {"gpuinfo",  (PyCFunction)at_gpuinfo, METH_VARARGS | METH_KEYWORDS,
                PyDoc_STR("gpuinfo()\n\n"
                          "Return lists of GPU present on the system.\n"
                          "[name,compute capability,stream processor number,multi processor number]\n"
                )},
        {"gpupass",  (PyCFunction)at_gpupass, METH_VARARGS | METH_KEYWORDS,
                PyDoc_STR("gpupass(line: Sequence[Element], r_in, n_turns: int, refpts: Uint32_refs = [], "
                          "reuse: Optional[bool] = False, omp_num_threads: Optional[int] = 0)\n\n"
                          "Track input particles r_in along line for nturns turns.\n"
                          "Record 6D phase space at elements corresponding to refpts for each turn.\n\n"
                          "Parameters:\n"
                          "    line:    list of elements\n"
                          "    rin:     6 x n_particles Fortran-ordered numpy array.\n"
                          "      On return, rin contains the final coordinates of the particles\n"
                          "    n_turns: number of turns to be tracked\n"
                          "    refpts:  numpy array of indices of elements where output is desired\n"
                          "       0 means entrance of the first element\n"
                          "       len(line) means end of the last element\n"
                          "    energy:  nominal energy [eV]\n"
                          "    particle (Optional[Particle]):  circulating particle\n"
                          "    reuse:   if True, use previously cached description of the lattice.\n"
                          "    losses:  if True, process losses\n\n"
                          "Returns:\n"
                          "    rout:    6 x n_particles x n_refpts x n_turns Fortran-ordered numpy array\n"
                          "         of particle coordinates\n\n"
                          ":meta private:"
                )},
        {NULL, NULL, 0, NULL} // Sentinel
};

PyMODINIT_FUNC PyInit_gpu(void) {

  static struct PyModuleDef moduledef = {
          PyModuleDef_HEAD_INIT,
          "gpu",         /* m_name */
          PyDoc_STR("GPU handler for Accelerator Toolbox"),      /* m_doc */
          -1,           /* m_size */
          AtGPUMethods, /* m_methods */
          NULL,         /* m_slots */
          NULL,         /* m_traverse */
          NULL,         /* m_clear */
          NULL,         /* m_free */
  };

  PyObject *m = PyModule_Create(&moduledef);

  // Import numpy array api
  if( PyArray_API == nullptr ) {
    if (_import_array() < 0) {
      PyErr_Print();
      return nullptr;
    }
  }

  // Init particle type
  PyObject *module = PyImport_ImportModule("at.lattice");
  if (module) {
    particle_type = PyObject_GetAttrString(module, "Particle");
    Py_DECREF(module);
    if(particle_type == nullptr) {
      cerr << "gpu module initialisation error, cannot get at.lattice.Particle type" << endl;
      return nullptr;
    }
  } else {
    PyErr_Print();
    return nullptr;
  }

  // Init Interface handler
  AbstractInterface::setHandler(new PyInterface());

  return m;
}

static PyObject *at_gpuinfo(PyObject *self, PyObject *args, PyObject *kwargs) {

  try {
    vector<GPU_INFO> gpuInfos;
    gpuInfos = AbstractGPU::getInstance()->getDeviceList();
    PyObject *infos = PyList_New(gpuInfos.size());
    for(int i=0;i<gpuInfos.size();i++) {
      PyObject *info = PyList_New(4);
      PyList_SetItem(info, 0, PyUnicode_FromString(gpuInfos[i].name.c_str()));
      PyList_SetItem(info, 1, PyUnicode_FromString(gpuInfos[i].version.c_str()));
      PyList_SetItem(info, 2, PyLong_FromLong(gpuInfos[i].smNumber));
      PyList_SetItem(info, 3, PyLong_FromLong(gpuInfos[i].mpNumber));
      PyList_SetItem(infos, i, info);
    }
    return infos;
  } catch (string& errorStr) {
    cout << "at_gpuinfo(): " << errorStr << endl;
    return PyList_New(0);
  }

}

static PyObject *at_gpupass(PyObject *self, PyObject *args, PyObject *kwargs) {

  static const char *kwlist[] = {"line","rin","nturns","refpts","turn",
                                 "energy", "particle", "keep_counter",
                                 "reuse","losses",
                                 "bunch_spos", "bunch_currents", nullptr};

  PyObject *lattice;
  PyObject *particle;
  PyObject *energy;
  PyArrayObject *rin;
  PyArrayObject *refs;
  PyArrayObject *bcurrents;
  PyArrayObject *bspos;
  int num_turns;
  int keep_lattice=0;
  int keep_counter=0;
  int counter=0;
  int losses=0;

  // Get input args
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!i|O!$iO!O!pppO!O!", const_cast<char **>(kwlist),
                                   &PyList_Type, &lattice,
                                   &PyArray_Type, &rin,
                                   &num_turns,
                                   &PyArray_Type, &refs,
                                   &counter,
                                   &PyFloat_Type, &energy,
                                   particle_type, &particle,
                                   &keep_counter,
                                   &keep_lattice,
                                   &losses,
                                   &PyArray_Type, &bspos,
                                   &PyArray_Type, &bcurrents)) {
    return nullptr;
  }

  size_t i;
  string code;

  try {

    PyInterface *pyI = (PyInterface *) AbstractInterface::getInstance();
    size_t nElements = PyList_Size(lattice);
    Lattice *l = new Lattice(nElements);
    for (i = 0; i < nElements; i++) {
      PyObject *elem = PyList_GET_ITEM(lattice, i);
      pyI->setObject(elem);
      l->addElement();
    }
    l->generateGPUKernel(code);
    cout << code << endl;
    AbstractGPU::getInstance()->run(code);

  } catch (string& errStr) {
    string err =  "at_gpupass() failed: " + errStr;
    return PyErr_Format(PyExc_RuntimeError,err.c_str());
  }

  return PyErr_Format(PyExc_RuntimeError, "Not yet implemented");

}

#endif