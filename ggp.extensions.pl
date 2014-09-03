
% For Stormur
library_directory('~/usr/local/share/Yap/').
%:- use_module( '~/usr/local/share/Yap/lists.yap' ).
%:- use_module( '~/usr/local/share/Yap/ordsets.yap' ).
% For local
:- use_module( library( lists ) ).
:- use_module( library( ordsets ) ).

distinct( _x, _y ) :- _x \= _y.  % NOTE: \== ??
or( _x, _y ) :- _x ; _y.
or( _x, _y, _z ) :- _x ; _y ; _z.
or( _x, _y, _z, _w ) :- _x ; _y ; _z ; _w.
or( _x, _y, _z, _w, _v ) :- _x ; _y ; _z ; _w ; _v.
or( _x, _y, _z, _w, _v, _q ) :- _x ; _y ; _z ; _w ; _v ; _q.
or( _x, _y, _z, _w, _v, _q, _r ) :- _x ; _y ; _z ; _w ; _v ; _q ; _r.
or( _x, _y, _z, _w, _v, _q, _r, _s ) :- _x ; _y ; _z ; _w ; _v ; _q ; _r ; _s.
or( _x, _y, _z, _w, _v, _q, _r, _s, _t ) :- _x ; _y ; _z ; _w ; _v ; _q ; _r ; _s ; _t.
or( _x, _y, _z, _w, _v, _q, _r, _s, _t, _u ) :- _x ; _y ; _z ; _w ; _v ; _q ; _r ; _s ; _t ; _u.

:- dynamic state/1.

add_state_clauses( [] ).
add_state_clauses( [_x | _l] ) :-
    assert( state( _x ) ),
    add_state_clauses( _l ).

add_does_clauses( [] ).

add_does_clauses( [ [_p,_m] | _l] ) :-
    assert( does( _p, _m ) ),
    add_does_clauses( _l ).

add_does_clause( _p, _m ) :-
    assert( does( _p, _m ) ).

get_does_clause(_p, _m) :-
	does(_p, _m).
	
state_init( _r ) :-
%    state_initialize,
    role( _r ).

state_retract_info( _l ) :-
    bagof( _c, state( _c ), _l ).

state_gen_moves( _p, _m ) :-
    legal( _p, _m ).

state_make_move( _p, _m ) :-
    assert( does( _p, _m ) ),
    bagof( A, next( A ), _l ),
    retract( does( _p, _m ) ),
    retractall( state( _ ) ),
    add_state_clauses( _l ).

state_make_sim_moves( _ml ) :-
    add_does_clauses( _ml ),
    bagof( A, next( A ), _l ),
    retractall( does( _, _ ) ),
    retractall( state( _ ) ),
    remove_duplicates( _l, _ll ),
    add_state_clauses( _ll ).

state_make_rel_moves( _ml ) :-
    add_does_clauses( _ml ),
    bagof( A, next( A ), _l ),
    bagof( B, state( B ), _ll ),
    retractall( does( _, _ ) ),
    retractall( state( _ ) ),
    append(_l, _ll, _lll),
    remove_duplicates( _lll, _llll ),
    add_state_clauses( _llll ).

state_make_exist_moves :-
    bagof( A, next( A ), _l ),
    retractall( does( _, _ ) ),
    retractall( state( _ ) ),
    remove_duplicates( _l, _ll ),
    add_state_clauses( _ll ).

state_retract_move( _ri ) :-
    retractall( state( _ ) ),
    add_state_clauses( _ri ).

state_append_move( _ri ) :-
    add_state_clauses( _ri ).

state_remove_duplicates :-
    state_retract_info( _l ),
    remove_duplicates( _l, _ll ),
    retractall( state( _ ) ),
    add_state_clauses( _ll ).

state_peek_next( _ml, _sl ) :-
    add_does_clauses( _ml ),
    bagof( A, next( A ), _l ),
    retractall( does( _, _ ) ),
    remove_duplicates( _l, _sl ).

state_effects_plus( _ml, _ps ) :-
    state_peek_next( _ml, _nl ),
    list_to_ord_set( _nl, _ns ),
    state_retract_info( _cl ),
    list_to_ord_set( _cl, _cs ),
    ord_subtract( _ns, _cs, _ps ).

state_effects_minus( _ml, _ms ) :-
% get next state
    state_peek_next( _ml, _nl ),
    list_to_ord_set( _nl, _ns ),
% subtract current state
    state_retract_info( _cl ),
    list_to_ord_set( _cl , _cs ),
    not(state_get_has_seen( _hl )),
    ord_subtract( _cs, _ns, _ms ).

state_effects_minus( _ml, _ms ) :-
% get next state
    state_peek_next( _ml, _nl ),
    list_to_ord_set( _nl, _ns ),
% subtract current state
    state_retract_info( _cl ),
    list_to_ord_set( _cl , _cs ),
% subtract has_seen if any
    state_get_has_seen( _hl ),
    list_to_ord_set( _hl, _hs ),
    ord_subtract( _cs, _ns, _ds ),
    ord_subtract( _ds, _hs, _ms ).

state_get_has_seen( _hl) :-
	bagof( has_seen( A ), state( has_seen( A ) ), _hl ).

state_is_terminal :-
    terminal.

state_goal( _p, _r ) :-
    goal( _p, _r ).

state_assert_clause( _x ) :-
    assert( state( _x ) ).

state_retract_clause( _x ) :-
    retract( state( _x ) ).

state_initialize :-
    bagof( A, state( A ), _l ),
    retractall( state( _ ) ),
    remove_duplicates( _l, _ll ),
    add_state_clauses( _ll ).

has_seen_function(state(X)) :-
    state(has_seen(X)).

has_seen_function(X) :-
    not(X).
