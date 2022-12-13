import numpy
from typing import Callable
import functools
from at.lattice import refpts_iterator, Lattice, AtError
from at.lattice import shift_elem, rotate_elem
from at.physics import find_orbit, get_optics
from at.tracking import lattice_pass
from scipy.stats import truncnorm, norm


__all__ = ['find_orbit_err', 'get_optics_err', 'enable_errors']


_BPM_ATTRS = ['BPMGain', 'BPMOffset', 'BPMTilt']
_ERR_ATTRS = ['PolynomBErr', 'PolynomAErr', 'ShiftErr', 'RotationErr']


def _truncated_randn(shape, sigma=1.0, mean=0.0, truncation=None, seed=None):
    if seed is not None:
        numpy.random.seed(seed)
    npoints = numpy.prod(shape)
    sigmas = numpy.broadcast_to(sigma, shape)
    means = numpy.broadcast_to(mean, shape)
    if truncation is not None:
        pts = truncnorm.rvs(-truncation, truncation, size=npoints)
    else:
        pts = norm.rvs(size=npoints)
    return numpy.reshape(pts, shape) * sigmas + means


def assign_errors(ring, key, truncation=None, seed=None, **kwargs):
    elements = ring.get_elements(key)
    for attr in _BPM_ATTRS+_ERR_ATTRS:
        val = kwargs.pop(attr, None)
        if val is not None:
            vals = numpy.broadcast_to(val, (len(elements),
                                            numpy.size([val])))                                            
            rv = _truncated_randn(vals.shape, sigma=vals,
                                  truncation=truncation, seed=seed)
            [setattr(e, attr, v) for e, v in zip(elements, rv)]


def _rotmat(theta):
    cs = numpy.cos(theta)
    sn = numpy.sin(theta)
    return numpy.array([[cs, -sn], [sn, cs]])


def _apply_bpm_orbit_error(ring, orbit, refpts):
    if refpts is None:
        return orbit
    for e, o6 in zip(refpts_iterator(ring, refpts), orbit):
        if hasattr(e, 'BPMGain'):
            o6[[0, 2]] *= e.BPMGain
        if hasattr(e, 'BPMOffset'):
            o6[[0, 2]] += e.BPMOffset
        if hasattr(e, 'BPMTilt'):
            o6[[0, 2]] = _rotmat(e.BPMTilt) @ o6[[0, 2]]
    return orbit


def _apply_bpm_track_error(ring, trajectory, refpts):
    if refpts is None:
        return trajectory
    for traj in trajectory.T:
        for e, o6 in zip(refpts_iterator(ring, refpts), traj):
            if hasattr(e, 'BPMGain'):
                o6[:, [0, 2]] *= e.BPMGain
            if hasattr(e, 'BPMOffset'):
                o6[:, [0, 2]] += e.BPMOffset
            if hasattr(e, 'BPMTilt'):
                o6[:, [0, 2]] = [_rotmat(e.BPMTilt) @ o2 
                                 for o2 in o6[:, [0, 2]]]
    return trajectory


def _apply_alignment_errors(ring, **kwargs):
    refpts = [(hasattr(e, 'ShiftErr') or hasattr(e, 'RotationErr'))
              for e in ring]
    ring = ring.replace(refpts)
    for e in ring[refpts]:
        shift = getattr(e, 'ShiftErr', None) 
        rots = getattr(e, 'RotationsErr', None)   
        if shift is not None:
            shiftx = kwargs.pop('shiftx', True)*shift[0]
            shifty = kwargs.pop('shifty', True)*shift[1]
            shift_elem(e, shiftx, shifty, relative=True)
        if rots is not None:
            tilt = kwargs.pop('tilt', True)*rots[0]
            pitch = kwargs.pop('pitch', True) * rots[1]
            yaw = kwargs.pop('yaw', True) * rots[2]
            rotate_elem(e, tilt=tilt, pitch=pitch,
                        yaw=yaw, relative=True)
    return ring


def _apply_field_errors(ring, **kwargs):
    def sanitize(e):
        mo = max(len(e.PolynomA), len(e.PolynomB))
        e.PolynomA = numpy.pad(e.PolynomA, mo - len(e.PolynomA))
        e.PolynomB = numpy.pad(e.PolynomB, mo - len(e.PolynomB))
        e.MaxOrder = mo - 1

    def get_pol(e, pname, index):
        perr = numpy.copy(getattr(e, pname + 'Err'))
        if index is not None:
            perr[range(len(perr)) != index] = 0.0
        le = sorted((getattr(e, pname), perr), key=len)
        pn = numpy.copy(le[1])
        pn[:len(le[0])] += le[0]
        return pn

    def set_polerr(ring, pname, index):
        refpts = [hasattr(e, pname) and hasattr(e, pname+'Err') for e in ring]
        rr = ring.replace(refpts)
        for e in rr[refpts]:
            setattr(e, pname, get_pol(e, pname, index))
            sanitize(e)
        return rr

    for pol in ['A', 'B']:
        if kwargs.pop('Polynom'+pol, True):
            index = kwargs.pop('Index'+pol, None)
            ring = set_polerr(ring, 'Polynom'+pol, index)
    return ring
    

def enable_errors(ring, **kwargs):
    if getattr(ring, '_has_errors', False): 
        raise AtError('Errors already enabled on this ring')
    else:
        ring = _apply_field_errors(ring, **kwargs)
        ring = _apply_alignment_errors(ring, **kwargs)
        ring._has_errors = True
    return ring


def _apply_errors(func) -> Callable:
    @functools.wraps(func)
    def wrapper(ring, *args, **kwargs):
        if not getattr(ring, '_has_errors', False):
            ring = enable_errors(ring)
        refpts = kwargs.get('refpts', None)
        if func is lattice_pass:
            rout = func(ring, *args, **kwargs)
            rout = _apply_bpm_track_error(ring, rout, refpts)
            return rout
        elif func is get_optics:
            ld0, bd, ld = func(ring, *args, **kwargs)
            ld.closed_orbit = _apply_bpm_orbit_error(ring, ld.closed_orbit,
                                                     refpts)
            return ld0, bd, ld
        elif func is find_orbit:
            orbit0, orbit = func(ring, *args, **kwargs)
            orbit = _apply_bpm_orbit_error(ring, orbit, refpts)
            return orbit0, orbit
        else:
            raise AtError('Error wrapper not available for {}'
                          .format(func.__name__))
    return wrapper


def get_mean_std_err(ring, key, attr, index=0):
    vals = [numpy.atleast_1d(getattr(e, attr, 0.0))[index]
            for e in ring.get_elements(key)]
    return numpy.mean(vals), numpy.std(vals)
    

find_orbit_err = _apply_errors(find_orbit)
Lattice.find_orbit_err = find_orbit_err
get_optics_err = _apply_errors(get_optics)
Lattice.get_optics_err = get_optics_err
lattice_pass_err = _apply_errors(lattice_pass)
Lattice.enable_errors = enable_errors
