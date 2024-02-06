function atclass=getclass_6d(elem)
%GETCLASS_6D    Private. Guess class for 6d motion
%
% Returns the element class ignoring its Class field when non-ambiguous
% Simplified and faster version of atguessclass

if isfield(elem,'BendingAngle')
    atclass='Bend';
elseif isfield(elem,'PolynomB') && elem.Length > 0
    if isfield(elem, 'Class')
        atclass=elem.Class;
    else
        maxorder=elem.MaxOrder+1;
        loworder=find(abs(elem.PolynomB(2:maxorder))~=0,1);
        if isempty(loworder)
            atclass='Drift';
        elseif loworder==1
            atclass='Quadrupole';
        elseif loworder==2
            atclass='Sextupole';
        else
            atclass='Multipole';
        end
    end
elseif isfield(elem,'Frequency')
    atclass='RFCavity';
elseif isfield(elem,'Class')
    atclass=elem.Class;
else
    atclass='Other';
end

end
