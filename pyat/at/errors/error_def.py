import numpy as np
from typing import Optional, Union
from at.lattice import refpts_iterator, Lattice, Refpts
from at.lattice import shift_elem, rotate_elem
from at.physics import find_orbit, get_optics
from at.tracking import lattice_pass
from scipy.stats import truncnorm, norm


__all__ = ['find_orbit_err', 'get_optics_err', 'lattice_pass_err',
           'assign_errors', 'enable_errors']

_BPM_ATTRS = ('BPMGain', 'BPMOffset', 'BPMTilt')
_ERR_ATTRS = ('PolynomBErr', 'PolynomAErr', 'ShiftErr', 'RotationErr')
_SEL_ARGS = ('all', 'PolynomAIndex', 'PolynomBIndex')


def _truncated_randn(truncation=None, **kwargs):
    if truncation is not None:
        return truncnorm.rvs(-truncation, truncation, **kwargs)
    else:
        return norm.rvs(**kwargs)


def assign_errors(ring: Lattice, key, truncation: Optional[float] = None,
                  seed: Optional[Union[int, np.random.Generator]] = None,
                  **kwargs):
    r"""Assign errors to selected elements

    The errors are stored as additional attributes but are not enabled for
    tracking (see :py:func:`enable_errors`).

    Args:
        ring:   Lattice description.
        key:    Element selector. May be:

                  #. an element instance, will return all elements of the same
                     type in the lattice, e.g. :pycode:`Drift('d1', 1.0)`
                  #. an element type, will return all elements of that type in
                     the lattice, e.g. :pycode:`at.Sextupole`
                  #. a string to match against elements' ``FamName``, supports
                     Unix shell-style wildcards, e.g. ``'BPM_*1'``
        truncation:     Truncation of the Gaussian error distribution at +/-
          *truncation* * :math:`\sigma`
        seed:   Seed for the random generator. It *seed* is :py:obj:`None`, the
          :py:obj:`numpy.random.RandomState` singleton is used. If seed is an
          :py:class:`int`, a new :py:class:`~numpy.random.RandomState` instance
          is used. If seed is already a :py:class:`~numpy.random.Generator`
          like :py:obj:`at.random.common` then that instance is used.

    Other keyword arguments specify the kind of errors to be assigned. Error
    definitions may be given in 2 forms:

    * **a tuple of values**: (*systematic*, *random*). The *systematic* value
      is applied unchanged to all the selected elements, the *random* value is
      taken as the standard deviation of the generated error distribution,
    * **a single value**: it acts as the *random* part of the previous form,
      and there is no systematic error.

    .. rubric:: Monitor errors:

    The beam positions returned at :py:class:`.Monitor` locations are computed
    by applying in order a tilt :math:`\theta`, gains :math:`g_x`, :math:`g_y`
    and offsets :math:`o_x`, :math:`o_y` resulting in:

    .. math::

       \begin{pmatrix} x_{monitor} \\ y_{monitor} \end{pmatrix} =
       \begin{pmatrix} o_x \\ o_y \end{pmatrix} +
       \begin{pmatrix} g_x & 0 \\ -0 & g_y \end{pmatrix}
       \begin{pmatrix} cos\theta & sin\theta \\
       -sin\theta & cos\theta \end{pmatrix}
       \begin{pmatrix} x_{beam} \\ y_{beam} \end{pmatrix}

    Note that this corresponds to the effect of:

    #. rotating the monitor device by :math:`\theta`,
    #. then applying the gains,
    #. and finally shifting the monitor device by :math:`-o_x` horizontally and
       :math:`-o_y` vertically

    Keyword Args:
        BPMOffset:  Offsets may be:

          * :py:class:`float` or (1,) float array: the same offset is applied
            in both planes to all elements
          * (2,) float array: [offset_x, offset_y] applied to all elements
          * (nelems, 1) or (nelems, 2) array: assign one value per element
            (may be useful for systematic errors)
        BPMGain:    Monitor gain (scaling factor)

          * :py:class:`float` or (1,) float array: the same gain is applied
            in both planes to all elements
          * (2,) float array: [gain_x, gain_y] applied to all elements
          * (nelems, 1) or (nelems, 2) array: assign one value per element
            (may be useful for systematic errors)
        BPMTilt:    :py:class:`float`, (1,) or (nelems, 1) float array

    .. rubric:: Magnet errors:

    Keyword Args:
        ShiftErr:       Magnet shift [m]. Maybe:

          * :py:class:`float` or (1,) float array: the same shift is applied
            in both planes to all elements
          * (2,) float array: [shift\ :sub:`x`, shift\ :sub:`y`\ ] applied to
            all elements
          * (nelems, 1) or (nelems, 2) array: assign one value per element
            (may be useful for systematic errors)
        RotationErr:    Magnet rotation

          * :py:class:`float` or (1,) float array: tilt error, no pitch nor yaw
          * (3,) float array: [tilt, pitch, yaw] rotations applied to all
            elements
          * (nelems, 1) or (nelems, 3) array: assign one value per element
            (may be useful for systematic errors)
        PolynomAErr:    Field error. *PolynomAErr* is added to the magnet's
          *PolynomA*
        PolynomBErr:    Field error. *PolynomBErr* is added to the magnet's
          *PolynomB*

    See also:
        :py:func:`enable_errors`, :py:func:`get_optics_err`
    """
    elements = ring.get_elements(key)
    for attr in _BPM_ATTRS + _ERR_ATTRS:
        val = kwargs.pop(attr, None)
        if val is not None:
            if isinstance(val, tuple):
                syst, rand = val
                rand = np.atleast_2d(rand)
                syst = np.atleast_2d(syst)
            else:
                rand = np.atleast_2d(val)
                syst = np.zeros(rand.shape)
            rv = _truncated_randn(size=(len(elements), rand.shape[-1]),
                                  truncation=truncation, random_state=seed)
            try:
                vals = syst + rv*rand
            except Exception as exc:
                exc.args = ('Attribute {0} does not accept value {1}: {2}'.
                            format(attr, val, exc),)
                raise
            for el, val in zip(elements, vals):
                setattr(el, attr, val)


def _rotmat(theta):
    cs = np.cos(theta)
    sn = np.sin(theta)
    return np.array([[cs, sn], [-sn, cs]])


def _apply_bpm_orbit_error(ring, refpts, orbit):
    for e, o6 in zip(refpts_iterator(ring, refpts), orbit):
        o6 = o6.reshape((-1, 6))
        if hasattr(e, 'BPMTilt'):
            o6[:, [0, 2]] = _rotmat(e.BPMTilt) @ o6[:, [0, 2]]
        if hasattr(e, 'BPMGain'):
            o6[:, [0, 2]] *= e.BPMGain
        if hasattr(e, 'BPMOffset'):
            o6[:, [0, 2]] += e.BPMOffset


def _apply_bpm_track_error(ring, refpts, trajectory):
    for traj in trajectory.T:
        _apply_bpm_orbit_error(ring, refpts, traj)


def _apply_alignment_errors(ring, **kwargs):
    refpts = [(hasattr(e, 'ShiftErr') or hasattr(e, 'RotationErr'))
              for e in ring]
    ring = ring.replace(refpts)
    alldef = kwargs.pop('all', True)
    for e in ring[refpts]:
        shift = getattr(e, 'ShiftErr')
        rots = getattr(e, 'RotationsErr')
        if shift is not None:
            shiftx = kwargs.pop('shiftx', alldef)*shift[0]
            shifty = kwargs.pop('shifty', alldef)*shift[1]
            shift_elem(e, shiftx, shifty, relative=True)
        if rots is not None:
            tilt = kwargs.pop('tilt', alldef)*rots[0]
            pitch = kwargs.pop('pitch', alldef) * rots[1]
            yaw = kwargs.pop('yaw', alldef) * rots[2]
            rotate_elem(e, tilt=tilt, pitch=pitch,
                        yaw=yaw, relative=True)
    return ring


def _apply_field_errors(ring, **kwargs):
    def sanitize(e):
        mo = max(len(e.PolynomA), len(e.PolynomB))
        e.PolynomA = np.pad(e.PolynomA, mo - len(e.PolynomA))
        e.PolynomB = np.pad(e.PolynomB, mo - len(e.PolynomB))
        e.MaxOrder = mo - 1

    def get_pol(e, pname, index):
        perr = np.copy(getattr(e, pname + 'Err'))
        if index is not None:
            perr[range(len(perr)) != index] = 0.0
        le = sorted((getattr(e, pname), perr), key=len)
        pn = np.copy(le[1])
        pn[:len(le[0])] += le[0]
        return pn

    def set_polerr(ring, pname, index):
        refpts = [hasattr(e, pname) and hasattr(e, pname+'Err') for e in ring]
        rr = ring.replace(refpts)
        for e in rr[refpts]:
            setattr(e, pname, get_pol(e, pname, index))
            sanitize(e)
        return rr

    alldef = kwargs.pop('all', True)
    for pol in ['A', 'B']:
        if kwargs.pop('Polynom'+pol, alldef):
            index = kwargs.pop('Index'+pol, None)
            ring = set_polerr(ring, 'Polynom'+pol, index)
    return ring
    

def enable_errors(ring: Lattice, **kwargs):
    r"""Enable magnet errors

    Returns a shallow copy of *ring* where magnet errors defined by
    :py:func:`assign_errors` are introduced in the magnet attributes.

    By default, all assigned errors are enabled, but keyword arguments allow a
    selection of the enabled errors.

    Args:
        ring:   Lattice description.

    Keyword Args:
        all (bool):         Set the default value for all the specific error
          flags. Default: :py:obj:`True`
        ShiftErr (bool):    enable magnet shift errors. Default: *all*
        RotationErr (bool): enable magnet rotation errors. Default: *all*
        PolynomAErr (bool): enable polynomial errors. Default: *all*
        PolynomAIndex (Optional[bool]): restrict the polynomial error to the
          specified index. Default: :py:obj:`None` meaning the whole polynom
        PolynomBErr (bool): enable polynomial errors. Default: *all*
        PolynomBIndex (Optional[bool]): restrict the polynomial error to the
          specified index. Default: :py:obj:`None` meaning the whole polynom

    Examples:
        >>> ringerr = enable_errors(ring)

        All errors are enabled in *ringerr*

        >>> ringerr = enable_errors(all=False, ShiftErr=True)

        Only magnet shift errors are active

        >>> ringerr = enable_errors(all=False)

        No magnet errors are enabled. Only monitor errors will be active.

    See also:
        :py:func:`assign_errors`, :py:func:`get_optics_err`
    """
    if not ring.has_errors:
        ring = _apply_field_errors(ring, **kwargs)
        ring = _apply_alignment_errors(ring, **kwargs)
        ring._has_errors = True
    return ring


def has_errors(ring):
    """Get the error status of a lattice"""
    return getattr(ring, '_has_errors', False)


def get_mean_std_err(ring: Lattice, key, attr, index=0):
    vals = [np.atleast_1d(getattr(e, attr, 0.0))[index]
            for e in ring.get_elements(key)]
    return np.mean(vals), np.std(vals)


def _sort_flags(kwargs):
    errargs = {}
    for key in _ERR_ATTRS + _SEL_ARGS:
        if key in kwargs:
            errargs[key] = kwargs.pop(key)
    return kwargs, errargs

def find_orbit_err(ring: Lattice, refpts: Refpts = None, **kwargs):
    """Find the closed orbit if a lattice with errors

    :py:func:`find_orbit_err` enables the selected errors, finds the closed
    orbit and modifies the result according to monitor errors.
    The *ring* :py:class:`~.Lattice` is not modified.

    Args:
        ring:       Lattice description
        refpts:

    See also:
        :py:func:`assign_errors`, :py:func:`get_optics_err`,
        :py:func:`lattice_pass_err`
    """
    kwargs, errargs = _sort_flags(kwargs)
    ring = enable_errors(ring, **errargs)
    orbit0, orbit = find_orbit(ring, refpts=refpts, **kwargs)
    _apply_bpm_orbit_error(ring, refpts, orbit)
    return orbit0, orbit


def get_optics_err(ring: Lattice, refpts: Refpts = None, **kwargs):
    """Linear analysis of a lattice with errors

    :py:func:`get_optics_err` enables the selected errors, makes the linear
    analysis of the modified :py:class:`~.Lattice` and modifies the closed
    orbit according to monitor errors.
    The *ring* :py:class:`~.Lattice` is not modified.

    Args:
        ring:       Lattice description
        refpts:

    See also:
        :py:func:`assign_errors`, :py:func:`find_orbit_err`,
        :py:func:`lattice_pass_err`

    """
    kwargs, errargs = _sort_flags(kwargs)
    ring = enable_errors(ring, **errargs)
    ld0, bd, ld = get_optics(ring, refpts=refpts, **kwargs)
    _apply_bpm_orbit_error(ring, refpts, ld.closed_orbit)
    return ld0, bd, ld


def lattice_pass_err(ring: Lattice, r_in, nturns: int = 1,
                     refpts: Refpts = None, **kwargs):
    """Tracking through a lattice with errors

    :py:func:`lattice_pass_err` enables the selected errors, and tracks through
    the modified :py:class:`~.Lattice`. The coordinates on monitors are modified
    according to monitor errors.
    The *ring* :py:class:`~.Lattice` is not modified.

    Parameters:
        lattice:                lattice description
        r_in:                   (6, N) array: input coordinates of N particles.
          *r_in* is modified in-place and reports the coordinates at
          the end of the element. For the best efficiency, *r_in*
          should be given as F_CONTIGUOUS numpy array.
        nturns:                 number of turns to be tracked
        refpts:                 numpy array of indices of elements where
          output is desired:

          * len(line) means end of the last element (default)
          * 0 means entrance of the first element

    See also:
        :py:func:`assign_errors`, :py:func:`get_optics_err`,
        :py:func:`find_orbit_err`

    """
    kwargs, errargs = _sort_flags(kwargs)
    ring = enable_errors(ring, **errargs)
    rout = lattice_pass(ring, r_in, nturns, refpts, **kwargs)
    _apply_bpm_track_error(ring, refpts, rout)
    return rout


Lattice.assign_errors = assign_errors
Lattice.find_orbit_err = find_orbit_err
Lattice.get_optics_err = get_optics_err
Lattice.enable_errors = enable_errors
Lattice.has_errors = property(has_errors, doc="Error status")
