function [p, varargout] = dmperm (G)
%DMPERM Dulmage-Mendelsohn permutation of a GraphBLAS matrix.
% See 'help dmperm' for details.

% SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2019, All Rights Reserved.
% http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

[p, varargout{1:nargout-1}] = builtin ('dmperm', logical (G)) ;

