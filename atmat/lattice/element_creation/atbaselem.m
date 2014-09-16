function elem = atbaselem(famname,method,varargin)
%ELEM=ATBASELEM(FAMNAME,METHOD,'FIELDNAME1',VALUE1,...) create AT element
%   Create an AT element structure and check the consistence of
%   PolynomA, PolynomB, MaxOrder and NumIntSteps

[rsrc,famname]=getatarg(varargin,famname,'FamName');
[rsrc,method]=getatarg(rsrc,method,'PassMethod');
elem=struct('FamName',famname,'PassMethod',method,rsrc{:});
ab=isfield(elem,{'PolynomA','PolynomB'});
if any(ab)
    if ~ab(1), elem.PolynomA=[]; end
    if ~ab(2), elem.PolynomB=[]; end
    if ~isfield(elem,'MaxOrder')
        elem.MaxOrder=max([1 find(abs(elem.PolynomB)>0,1,'last') find(abs(elem.PolynomA)>0,1,'last')])-1;
    end
    la=length(elem.PolynomA);
    if la < elem.MaxOrder+1, elem.PolynomA=[elem.PolynomA zeros(1,elem.MaxOrder+1-la)]; end
    lb=length(elem.PolynomB);
    if lb < elem.MaxOrder+1, elem.PolynomB=[elem.PolynomB zeros(1,elem.MaxOrder+1-lb)]; end
    if isfield(elem,'Length') && elem.Length ~= 0 && ~isfield(elem,'NumIntSteps')
        elem.NumIntSteps=10;
    end
end
end
