'use strict';
'require view';
'require fs';
'require ui';
'require rpc';

var callSfpStatus = rpc.declare({
	object: 'sfp',
	method: 'getStatus',
	expect: { sfp: {} }
});

return view.extend({
	
	load: function() {
		return Promise.all([
			L.resolveDefault(callSfpStatus(), {}),			
		]);
	},

	updateTable: function(table, sfp, port) {
		var rows = [];
		
		rows.push([_("Present"),sfp[port].present]);
		rows.push([_("Los"),sfp[port].los]);
		rows.push([_("Vendor"),sfp[port].vendor]);		
		rows.push([_("Vendor OUI"),sfp[port].vendor_oui]);		
		rows.push([_("Vendor PN"),sfp[port].vendor_pn]);
		rows.push([_("Vendor REV"),sfp[port].vendor_rev]);
		rows.push([_("Identifier"),sfp[port].identifier]);		
		rows.push([_("Connector"),sfp[port].connector]);		
		rows.push([_("Type"),sfp[port].type]);		
		rows.push([_("Link length"),sfp[port].link_len]);		
		rows.push([_("Fiber technology"),sfp[port].fiber_tec]);
		rows.push([_("Media"),sfp[port].media]);		
		rows.push([_("Speed"),sfp[port].speed]);		
		rows.push([_("Encoding"),sfp[port].encoding]);		
		rows.push([_("Wave length"),sfp[port].wavelen]);		
		rows.push([_("NBR"),sfp[port].nbr]);		
		rows.push([_("len9"),sfp[port].len9]);		
		rows.push([_("len50"),sfp[port].len50]);
		rows.push([_("len62"),sfp[port].len62]);
		rows.push([_("lenc"),sfp[port].lenc]);		
		rows.push([_("Temperature"),sfp[port].temper]);
		rows.push([_("Voltage"),sfp[port].voltage]);
		rows.push([_("TX Bias"),sfp[port].tx_bias]);
		rows.push([_("TX Power"),sfp[port].tx_power]);
		rows.push([_("RX Power"),sfp[port].rx_power]);		
			
		cbi_update_table(table, rows, E('em', _('No information available')));
	},

	render: function(data) {

		var sfp1 = E('table', { 'class': 'table' }, [
			E('tr', { 'class': 'tr table-titles' }, [
				E('th', { 'class': 'th col-2 left' }, _('Name')),
				E('th', { 'class': 'th col-10 left' }, _('Value'))
			])
		]);
		var sfp2 = E('table', { 'class': 'table' }, [
			E('tr', { 'class': 'tr table-titles' }, [
				E('th', { 'class': 'th col-2 left' }, _('Name')),
				E('th', { 'class': 'th col-10 left' }, _('Value'))
			])
		]);
	
		this.updateTable(sfp1, data[0],0);
		this.updateTable(sfp2, data[0],1);

		var view = E('div', {}, [
			E('h2', _('SFP Statistics')),
			E('div', { 'class': 'cbi-map-descr' }, _('Detailed information about the connected SFP module(DDM, manufactor, type, etc).')),

			E('div', {}, [
				E('div', { 'data-tab': 'init', 'data-tab-title': _('SFP1') }, [
					E('p', {}, _('')),
					sfp1
				]),
				E('div', { 'data-tab': 'init2', 'data-tab-title': _('SFP2') }, [
					E('p', {}, _('')),
					sfp2
				])
			])
		]);

		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		return view;		
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
