CREATE TABLE `pe_sd2_customcost` (
  `entry` int(11) unsigned NOT NULL,
  `itemOrGroup0` int(11) NOT NULL DEFAULT '0' COMMENT 'itemID if positive, ItemGroup ID if negative (see pe_sd2_itemgroup table). 0=nothing.',
  `count0` int(11) unsigned NOT NULL DEFAULT '0',
  `itemOrGroup1` int(11) NOT NULL DEFAULT '0',
  `count1` int(11) unsigned NOT NULL DEFAULT '0',
  `itemOrGroup2` int(11) NOT NULL DEFAULT '0',
  `count2` int(11) unsigned NOT NULL DEFAULT '0',
  `itemOrGroup3` int(11) NOT NULL DEFAULT '0',
  `count3` int(11) unsigned NOT NULL DEFAULT '0',
  `itemOrGroup4` int(11) NOT NULL DEFAULT '0',
  `count4` int(11) unsigned NOT NULL DEFAULT '0',
  `money` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

CREATE TABLE `pe_sd2_customvendor` (
  `entry` int(11) NOT NULL COMMENT 'NPC entry',
  `item` int(11) NOT NULL COMMENT 'Item ID to be sold',
  `count` smallint(5) unsigned NOT NULL DEFAULT '1' COMMENT 'count of items sold',
  `cost_id` int(11) NOT NULL COMMENT 'entry in table pe_sd2_customcost',
  `fmt` varchar(100) DEFAULT '',
  PRIMARY KEY (`entry`,`item`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

CREATE TABLE `pe_sd2_itemgroup` (
  `groupId` int(11) unsigned NOT NULL,
  `item` int(11) unsigned NOT NULL,
  PRIMARY KEY (`groupId`,`item`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

CREATE TABLE `pe_sd2_itemgroupname` (
  `groupId` int(11) unsigned NOT NULL,
  `name` varchar(50) NOT NULL,
  PRIMARY KEY (`groupId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
