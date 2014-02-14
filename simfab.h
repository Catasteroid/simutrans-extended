/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic license.
 * (see license.txt)
 */

#ifndef simfab_h
#define simfab_h

#include "dataobj/koord3d.h"
#include "dataobj/translator.h"

#include "tpl/slist_tpl.h"
#include "tpl/vector_tpl.h"
#include "tpl/array_tpl.h"
#include "tpl/inthashtable_tpl.h"
#include "besch/fabrik_besch.h"
#include "halthandle_t.h"
#include "simworld.h"
#include "utils/plainstring.h"


class spieler_t;
class stadt_t;
class ware_t;


/**
 * Factory statistics
 * @author Knightly
 */
#define MAX_MONTH                  (12)
#define FAB_PRODUCTION              (0)
#define FAB_POWER                   (1)
#define FAB_BOOST_ELECTRIC          (2)
#define FAB_BOOST_PAX               (3)
#define FAB_BOOST_MAIL              (4)
#define FAB_PAX_GENERATED           (5)
#define FAB_PAX_DEPARTED            (6)
#define FAB_PAX_ARRIVED             (7)
#define FAB_MAIL_GENERATED          (8)
#define FAB_MAIL_DEPARTED           (9)
#define FAB_MAIL_ARRIVED           (10)
#define MAX_FAB_STAT               (11)

// reference lines
#define FAB_REF_MAX_BOOST_ELECTRIC  (0)
#define FAB_REF_MAX_BOOST_PAX       (1)
#define FAB_REF_MAX_BOOST_MAIL      (2)
#define FAB_REF_DEMAND_ELECTRIC     (3)
#define FAB_REF_DEMAND_PAX          (4)
#define FAB_REF_DEMAND_MAIL         (5)
#define MAX_FAB_REF_LINE            (6)

// statistics for goods
#define MAX_FAB_GOODS_STAT          (4)
// common to both input and output goods
#define FAB_GOODS_STORAGE           (0)
// input goods
#define FAB_GOODS_RECEIVED          (1)
#define FAB_GOODS_CONSUMED          (2)
#define FAB_GOODS_TRANSIT           (3)
// output goods
#define FAB_GOODS_DELIVERED         (1)
#define FAB_GOODS_PRODUCED          (2)


// production happens in every second
#define PRODUCTION_DELTA_T (1024)
// default production factor
#define DEFAULT_PRODUCTION_FACTOR_BITS (8)
#define DEFAULT_PRODUCTION_FACTOR (1 << DEFAULT_PRODUCTION_FACTOR_BITS)
// precision of apportioned demand (i.e. weights of factories at target cities)
#define DEMAND_BITS (4)


/**
 * Convert internal values to displayed values
 */
sint64 convert_goods(sint64 value);
sint64 convert_power(sint64 value);
sint64 convert_boost(sint64 value);

// to prepare for 64 precision ...
class ware_production_t
{
private:
	const ware_besch_t *type;
	// Knightly : statistics for each goods
	sint64 statistics[MAX_MONTH][MAX_FAB_GOODS_STAT];
	sint64 weighted_sum_storage;

	/// clears statistics, transit, and weighted_sum_storage
	void init_stats();
public:
	ware_production_t() : type(NULL), menge(0), max(0), transit(0), max_transit(0), index_offset(0)
	{
#ifdef TRANSIT_DISTANCE
		count_suppliers = 0;
#endif
		init_stats();
	}

	const ware_besch_t* get_typ() const { return type; }
	void set_typ(const ware_besch_t *t) { type=t; }

	// Knightly : functions for manipulating goods statistics
	void roll_stats(sint64 aggregate_weight);
	void rdwr(loadsave_t *file);
	const sint64* get_stats() const { return *statistics; }
	void book_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_GOODS_STAT); statistics[0][stat_type] += value; }
	void set_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_GOODS_STAT); statistics[0][stat_type] = value; }
	sint64 get_stat(int month, int stat_type) const { assert(stat_type<MAX_FAB_GOODS_STAT); return statistics[month][stat_type]; }

	/**
	 * convert internal units to displayed values
	 */
	sint64 get_stat_converted(int month, int stat_type) const {
		assert(stat_type<MAX_FAB_GOODS_STAT);
		sint64 value = statistics[month][stat_type];
		if (stat_type==FAB_GOODS_STORAGE  ||  stat_type==FAB_GOODS_CONSUMED) {
			value = convert_goods(value);
		}
		return value;
	}
	void book_weighted_sum_storage(sint64 delta_time);

	sint32 menge;	// in internal units shifted by precision_bits (see step)
	sint32 max;
	sint32 transit;
	sint32 max_transit;	// current limit, depending of suppliers mean distance
#ifdef TRANSIT_DISTANCE
	sint32 count_suppliers;	// only needed for averaging
#endif

	uint32 index_offset; // used for haltlist and lieferziele searches in verteile_waren to produce round robin results
};


/**
 * Eine Klasse f�r Fabriken in Simutrans. Fabriken produzieren und
 * verbrauchen Waren und beliefern nahe Haltestellen.
 *
 * Die Abfragefunktionen liefern -1 wenn eine Ware niemals
 * hergestellt oder verbraucht wird, 0 wenn gerade nichts
 * hergestellt oder verbraucht wird und > 0 sonst
 * (entspricht Vorrat/Verbrauch).
 * 
 * A class of factories in Simutrans.
 *  Factories produce and consume goods and supplies near bus stops. 
 * The query functions return -1 if a product is never produced or consumed, 
 * 0 when nothing is manufactured or consumed and> 0 otherwise (equivalent to stocks / consumption).
 * @date 1998
 * @see haltestelle_t
 * @author Hj. Malthaner
 */
class fabrik_t
{
public:
	/**
	 * Konstanten
	 * @author Hj. Malthaner
	 */
	enum { precision_bits = 10, old_precision_bits = 10, precision_mask = 1023 };

private:

	gebaeude_t* building;
		
	/**
	 * Factory statistics
	 * @author Knightly
	 */
	sint64 statistics[MAX_MONTH][MAX_FAB_STAT];
	sint64 weighted_sum_production;
	sint64 weighted_sum_boost_electric;
	sint64 weighted_sum_boost_pax;
	sint64 weighted_sum_boost_mail;
	sint64 weighted_sum_power;
	sint64 aggregate_weight;

	// Knightly : Functions for manipulating factory statistics
	void init_stats();
	void set_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_STAT); statistics[0][stat_type] = value; }

	// Knightly : For accumulating weighted sums for average statistics
	void book_weighted_sums(sint64 delta_time);

	/**
	 * Die m�glichen Lieferziele
	 * 
	 * The possible delivery targets
	 * @author Hj. Malthaner
	 */
	vector_tpl <koord> lieferziele;
	uint32 lieferziele_active_last_month;

	/**
	 * suppliers to this factry
	 * @author hsiegeln
	 */
	vector_tpl <koord> suppliers;

	/**
	 * fields of this factory (only for farms etc.)
	 * @author prissi/Knightly
	 */
	struct field_data_t
	{
		koord location;
		uint16 field_class_index;

		field_data_t() : field_class_index(0) {}
		explicit field_data_t(const koord loc) : location(loc), field_class_index(0) {}
		field_data_t(const koord loc, const uint16 class_index) : location(loc), field_class_index(class_index) {}

		bool operator==(const field_data_t &other) const { return location==other.location; }
	};
	vector_tpl <field_data_t> fields;

	/**
	 * Die erzeugten waren auf die Haltestellen verteilen
	 * The produced were distributed at the stops
	 * @author Hj. Malthaner
	 */
	void verteile_waren(const uint32 produkt);

	// List of target cities
	/*vector_tpl<stadt_t *> target_cities;*/

	spieler_t *besitzer_p;		// player_t* owner_p
	static karte_ptr_t welt;

	const fabrik_besch_t *besch;

protected:

	/**
	 * Freight halts within range
	 */
	vector_tpl<nearby_halt_t> nearby_freight_halts;

private:
	/**
	 * Passenger halts within range
	 */
	vector_tpl<nearby_halt_t> nearby_passenger_halts;
	/**
	 * Mail halts within range
	 */
	vector_tpl<nearby_halt_t> nearby_mail_halts;

	/**
	 * Bauposition gedreht?
	 * Building position turned?
	 * @author V.Meyer
	 */
	uint8 rotate;

	/**
	 * produktionsgrundmenge
	 * "production fundamental set"
	 * @author Hj. Malthaner
	 */
	sint32 prodbase;

	/**
	 * multiplikator f�r die Produktionsgrundmenge
	 * multiplier for the production of basic quantity
	 * @author Hj. Malthaner
	 */
	sint32 prodfactor_electric;
	sint32 prodfactor_pax;
	sint32 prodfactor_mail;

	array_tpl<ware_production_t> eingang; ///< array for input/consumed goods
	array_tpl<ware_production_t> ausgang; ///< array for output/produced goods

	// The adjusted "max intransit percentage" for each type of input goods
	// indexed against the catg of each "eingang" (the input goods).
	inthashtable_tpl<uint8, uint16> max_intransit_percentages;

	/**
	 * Zeitakkumulator f�r Produktion
	 * Time accumulator for production
	 * @author Hj. Malthaner
	 */
	sint32 delta_sum;
	uint32 delta_menge;

	// production remainder when scaled to PRODUCTION_DELTA_T. added back next step to eliminate cumulative error
	uint32 menge_remainder;

	// Knightly : number of rounds where there is active production or consumption
	uint8 activity_count;

	// true if the factory has a transformer adjacent
	leitung_t* transformer_connected;

	// true, if the factory did produce enough in the last step to require power
	bool currently_producing;

	// power that can be currently drawn from this station (or the amount delivered)
	uint32 power;

	// power requested for next step
	uint32 power_demand;

	uint32 total_input, total_transit, total_output;
	uint8 status;

	/// Position of a building of the factory.
	koord3d pos;

	/// Position of the nw-corner tile of the factory.
	koord3d pos_origin;

	/**
	 * Number of times the factory has expanded so far
	 * Only for factories without fields
	 * @author Knightly
	 */
	uint16 times_expanded;

	/**
	 * Electricity amount scaled with prodbase
	 * @author TurfIt
	 */
	uint32 scaled_electric_amount;

	/**
	 * Pax/mail demand scaled with prodbase and month length
	 * @author Knightly
	 */
	uint32 scaled_pax_demand;
	uint32 scaled_mail_demand;

	/**
	 * Update scaled electricity amount
	 * @author TurfIt
	 */
	void update_scaled_electric_amount();

	/**
	 * Update scaled pax/mail demand
	 * @author Knightly
	 */
	void update_scaled_pax_demand();
	void update_scaled_mail_demand();

	/**
	 * Update production multipliers for pax and mail
	 * @author Knightly
	 */
	void update_prodfactor_pax();
	void update_prodfactor_mail();

	/**
	 * Recalculate storage capacities based on prodbase or capacities contributed by fields
	 * @author Knightly
	 */
	void recalc_storage_capacities();

	/**
	 * Class for collecting arrival data and calculating pax/mail boost with fixed period length
	 * @author Knightly
	 */
	#define PERIOD_BITS   (18)				// determines period length on which boost calculation is based
	#define SLOT_BITS     (6)				// determines the number of time slots available
	#define SLOT_COUNT    (1<<SLOT_BITS)	// number of time slots for accumulating arrived pax/mail
	class arrival_statistics_t
	{
	private:
		uint16 slots[SLOT_COUNT];
		uint16 current_slot;
		uint16 active_slots;		// number of slots covered since aggregate arrival last increased from 0 to +ve
		uint32 aggregate_arrival;
		uint32 scaled_demand;
	public:
		void init();
		void rdwr(loadsave_t *file);
		void set_scaled_demand(const uint32 demand) { scaled_demand = demand; }
		#define ARRIVALS_CHANGED (1)
		#define ACTIVE_SLOTS_INCREASED (2)
		sint32 advance_slot();
		void book_arrival(const uint16 amount);
		uint16 get_active_slots() const { return active_slots; }
		uint32 get_aggregate_arrival() const { return aggregate_arrival; }
		uint32 get_scaled_demand() const { return scaled_demand; }
	};

	void update_transit_intern( const ware_t& ware, bool add );

	/**
	 * Arrival data for calculating pax/mail boost
	 * @author Knightly
	 */
	arrival_statistics_t arrival_stats_pax;
	arrival_statistics_t arrival_stats_mail;

	plainstring name;

	/**
	 * For advancement of slots for boost calculation
	 * @author Knightly
	 */
	sint32 delta_slot;

	void recalc_factory_status();

	// create some smoke on the map
	void smoke() const;

	// scales the amount of production based on the amount already in storage
	uint32 scale_output_production(const uint32 product, uint32 menge) const;

	// This is the city within whose city limits the factory is located.
	// NULL if it is outside a city. This is re-checked monthly.
	// @author: jamespetts
	stadt_t* city;

	bool has_calculated_intransit_percentages;

protected:

	void delete_all_fields();

public:
	fabrik_t(loadsave_t *file);
	fabrik_t(koord3d pos, spieler_t* sp, const fabrik_besch_t* fabesch, sint32 initial_prod_base);
	~fabrik_t();

	gebaeude_t* get_building();

	/**
	 * Return/book statistics
	 * @author Knightly
	 */
	const sint64* get_stats() const { return *statistics; }
	sint64 get_stat(int month, int stat_type) const { assert(stat_type<MAX_FAB_STAT); return statistics[month][stat_type]; }
	void book_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_STAT); statistics[0][stat_type] += value; }


	static void update_transit( const ware_t& ware, bool add );

	/**
	 * convert internal units to displayed values
	 */
	sint64 get_stat_converted(int month, int stat_type) const {
		assert(stat_type<MAX_FAB_STAT);
		sint64 value = statistics[month][stat_type];
		switch(stat_type) {
			case FAB_POWER:
				value = convert_power(value);
				break;
			case FAB_BOOST_ELECTRIC:
			case FAB_BOOST_PAX:
			case FAB_BOOST_MAIL:
				value = convert_boost(value);
				break;
			default: ;
		}
		return value;
	}

	static fabrik_t * get_fab(const koord &pos);

	/**
	 * @return vehicle description object
	 * @author Hj. Malthaner
	 */
	const fabrik_besch_t *get_besch() const {return besch; }

	void laden_abschliessen();

	/**
	* gets position of a building belonging to factory
	*/
	koord3d get_pos() const { return pos; }

	void rotate90( const sint16 y_size );

	const vector_tpl<koord>& get_lieferziele() const { return lieferziele; }
	bool is_active_lieferziel( koord k ) const;

	const vector_tpl<koord>& get_suppliers() const { return suppliers; }

	/**
	 * Recalculate nearby halts
	 * These are stashed, so must be recalced when
	 * halts are built or destroyed
	 * @author neroden
	 */
	void recalc_nearby_halts();

	/**
	 * Re-mark nearby roads.
	 * Needs to be called by fabrikbauer_t (otherwise private).
	 */
	void mark_connected_roads(bool del);

	/**
	 * Functions for manipulating the list of connected cities
	 * @author Hj. Malthaner/prissi/Knightly
	 */
	/*void add_target_city(stadt_t *const city);
	void remove_target_city(stadt_t *const city);
	void clear_target_cities();
	const vector_tpl<stadt_t *>& get_target_cities() const { return target_cities; }*/

	/**
	 * F�gt ein neues Lieferziel hinzu
	 * Adds a new delivery goal
	 * @author Hj. Malthaner
	 */
	void  add_lieferziel(koord ziel);
	void  rem_lieferziel(koord pos);

	bool disconnect_consumer(koord pos);
	bool disconnect_supplier(koord pos);

	/**
	 * adds a supplier
	 * @author Hj. Malthaner
	 */
	void  add_supplier(koord pos);
	void  rem_supplier(koord pos);

	/**
	 * @return menge der ware typ ("quantity of the goods type")
	 *   -1 wenn typ nicht produziert wird ("if not type is produced")
	 *   sonst die gelagerte menge ("otherwise the stored quantity")
	 */
	sint32 input_vorrat_an(const ware_besch_t *ware);        // Vorrat von Warentyp ("Inventories of product")
	sint32 vorrat_an(const ware_besch_t *ware);        // Vorrat von Warentyp

	/**
	* returns all power and consume it to prevent multiple pumpes
	*/
	uint32 get_power() { uint32 p=power; power=0; return p; }

	/**
	* returns power wanted by the factory for next step and sets to 0 to prevent multiple senkes on same powernet
	*/
	uint32 step_power_demand() { uint32 p=power_demand; power_demand=0; return p; }
	uint32 get_power_demand() const { return power_demand; }

	/**
	*give power to the factory to consume ...
	*/
	void add_power(uint32 p) { power += p; }

	/**
	* senkes give back wanted power they can't supply such that a senke on a different powernet can try suppling
	* WARNING: senke stepping order can vary between ingame construction and savegame loading => different results after saving/loading the game
	*/
	void add_power_demand(uint32 p) { power_demand +=p; }

	/**
	* True if there was production requiring power in the last step
	*/
	bool is_currently_producing() const { return currently_producing; }

	/**
	* Used to limit transformers to one per factory
	*/
	bool is_transformer_connected() const { return (bool)transformer_connected || ((bool)city && !besch->is_electricity_producer()); }
	leitung_t* get_transformer_connected() { return transformer_connected; }
	void set_transformer_connected(leitung_t* connected) { transformer_connected = connected; }

	/**
	 * @return 1 wenn verbrauch,
	 * 0 wenn Produktionsstopp,
	 * -1 wenn Ware nicht verarbeitet wird
	 */
	sint8 is_needed(const ware_besch_t *) const;

	sint32 liefere_an(const ware_besch_t *, sint32 menge);

	void step(long delta_t);                  // fabrik muss auch arbeiten ("factory must also work")

	void neuer_monat();

	char const* get_name() const;
	void set_name( const char *name );

	sint32 get_kennfarbe() const { return besch->get_kennfarbe(); }

	spieler_t *get_besitzer() const
	{
		grund_t const* const p = welt->lookup(pos);
		return p ? p->first_obj()->get_besitzer() : 0;
	}

	void zeige_info();

	/**
	 * infostring on production
	 */
	void info_prod(cbuffer_t& buf) const;

	/**
	 * infostring on targets/sources
	 */
	void info_conn(cbuffer_t& buf) const;

	void rdwr(loadsave_t *file);

	/*
	 * Fills the vector with the koords of the tiles.
	 */
	void get_tile_list( vector_tpl<koord> &tile_list ) const;

	/**
	 * gibt eine NULL-Terminierte Liste von Fabrikpointern zur�ck
	 *
	 * a zero-scheduled list of factory pointers returns
	 * @author Hj. Malthaner
	 */
	static vector_tpl<fabrik_t *> & sind_da_welche(koord min, koord max);

	/**
	 * gibt true zurueck wenn sich ein fabrik im feld befindet
	 * 
	 * "gives true back if factory in the field is"
	 *
	 * @author Hj. Malthaner
	 */
	//static bool ist_da_eine(karte_t *welt, koord min, koord max);
	//static bool ist_bauplatz(karte_t *welt, koord pos, koord groesse, bool water, climate_bits cl);

	// hier die methoden zum parametrisieren der Fabrik
	// "here the methods to parameterize the factory"

	/**
	 * Baut die Geb�ude f�r die Fabrik
	 *
	 * "Build the buildings for the factory"
	 *
	 * @author Hj. Malthaner, V. Meyer
	 */
	void baue(sint32 rotate, bool build_fields, bool force_initial_prodbase);

	sint16 get_rotate() const { return rotate; }

	/* field generation code
	 * spawns a field for sure if probability>=1000
	 * @author Kieron Green
	 */
	bool add_random_field(uint16 probability);

	void remove_field_at(koord pos);

	uint32 get_field_count() const { return fields.get_count(); }

	/**
	 * total and current procduction/storage values
	 * @author Hj. Malthaner
	 */
	const array_tpl<ware_production_t>& get_eingang() const { return eingang; }
	const array_tpl<ware_production_t>& get_ausgang() const { return ausgang; }

	/**
	 * Production multipliers
	 * @author Hj. Malthaner
	 */
	sint32 get_prodfactor_electric() const { return prodfactor_electric; }
	sint32 get_prodfactor_pax() const { return prodfactor_pax; }
	sint32 get_prodfactor_mail() const { return prodfactor_mail; }
	sint32 get_prodfactor() const { return DEFAULT_PRODUCTION_FACTOR + prodfactor_electric + prodfactor_pax + prodfactor_mail; }

	/* does not takes month length into account */
	sint32 get_base_production() const { return prodbase; }
	void set_base_production(sint32 p);

	// TODO: Consider refctoring so as to avoid calling this method every step (although preliminary tests indicate that this does not seem to cause much slow-down compared to the previous method).
	sint32 get_current_production() const { return (sint32) (welt->calc_adjusted_monthly_figure(((sint64)prodbase * (sint64)(DEFAULT_PRODUCTION_FACTOR + prodfactor_electric + prodfactor_pax + prodfactor_mail)))) >> 8l; }

	/* prissi: returns the status of the current factory, as well as output */
	enum { bad, medium, good, inactive, nothing };
	static unsigned status_to_color[5];

	uint8  get_status() const { return status; }
	uint32 get_total_in() const { return total_input; }
	uint32 get_total_transit() const { return total_transit; }
	uint32 get_total_out() const { return total_output; }

	/**
	 * Crossconnects all factories
	 * @author prissi
	 */
	void add_all_suppliers();

	/* adds a new supplier to this factory
	 * fails if no matching goods are there
	 */
	bool add_supplier(fabrik_t* fab);

	/* adds a new customer to this factory
	 * fails if no matching goods are accepted
	 */
	bool add_customer(fabrik_t* fab);

	stadt_t* get_city() const { return city; }
	void clear_city() { city = NULL; }
	/**
	 * Return the scaled electricity amount and pax/mail demand
	 * @author Knightly
	 */
	uint32 get_scaled_electric_amount() const { return scaled_electric_amount; }
	uint32 get_scaled_pax_demand() const { return scaled_pax_demand; }
	uint32 get_scaled_mail_demand() const { return scaled_mail_demand; }

	bool is_end_consumer() const { return (ausgang.empty() && !besch->is_electricity_producer()); }

	/**
	 * Returns a list of goods produced by this factory.
	 */
	slist_tpl<const ware_besch_t*> *get_produced_goods() const;

	void add_to_world_list();

	inline uint32 get_base_pax_demand() const { return arrival_stats_pax.get_scaled_demand(); }
	inline uint32 get_base_mail_demand() const { return arrival_stats_mail.get_scaled_demand(); }

	void calc_max_intransit_percentages();
	// Average journey time to delivery goods of this type
	uint32 get_lead_time (const ware_besch_t* wtype);
	// Time to consume the full input store of these goods at full capacity
	uint32 get_time_to_consume_stock(uint32 index);

};

#endif
