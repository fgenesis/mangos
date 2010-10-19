CREATE TABLE `www_onlinestats` (
  `time` int(16) unsigned NOT NULL DEFAULT '0' COMMENT 'unixtime',
  `realmid` smallint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'mangos realm id',
  `a` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'alliance players',
  `h` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'horde players',
  PRIMARY KEY (`time`,`realmid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `autobroadcast` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `enabled` tinyint(4) NOT NULL DEFAULT '1',
  `text` longtext NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=24 DEFAULT CHARSET=utf8;


CREATE TABLE `creature_extended` (
  `entry` int(11) unsigned NOT NULL DEFAULT '0',
  `size` float NOT NULL DEFAULT '0',
  `minloot` int(11) unsigned NOT NULL DEFAULT '0',
  `SpellDmgMulti` float unsigned NOT NULL DEFAULT '1',
  `XPMulti` float unsigned NOT NULL DEFAULT '1',
  `honor` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED COMMENT='Creature extended data';

CREATE TABLE `creature_ai_scripts_custom` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT 'Identifier',
  `creature_id` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Template Identifier',
  `event_type` tinyint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Event Type',
  `event_inverse_phase_mask` int(11) NOT NULL DEFAULT '0' COMMENT 'Mask which phases this event will not trigger in',
  `event_chance` int(3) unsigned NOT NULL DEFAULT '100',
  `event_flags` int(3) unsigned NOT NULL DEFAULT '0',
  `event_param1` int(11) NOT NULL DEFAULT '0',
  `event_param2` int(11) NOT NULL DEFAULT '0',
  `event_param3` int(11) NOT NULL DEFAULT '0',
  `event_param4` int(11) NOT NULL DEFAULT '0',
  `action1_type` tinyint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Action Type',
  `action1_param1` int(11) NOT NULL DEFAULT '0',
  `action1_param2` int(11) NOT NULL DEFAULT '0',
  `action1_param3` int(11) NOT NULL DEFAULT '0',
  `action2_type` tinyint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Action Type',
  `action2_param1` int(11) NOT NULL DEFAULT '0',
  `action2_param2` int(11) NOT NULL DEFAULT '0',
  `action2_param3` int(11) NOT NULL DEFAULT '0',
  `action3_type` tinyint(5) unsigned NOT NULL DEFAULT '0' COMMENT 'Action Type',
  `action3_param1` int(11) NOT NULL DEFAULT '0',
  `action3_param2` int(11) NOT NULL DEFAULT '0',
  `action3_param3` int(11) NOT NULL DEFAULT '0',
  `comment` varchar(255) NOT NULL DEFAULT '' COMMENT 'Event Comment',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=4197 DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED COMMENT='EventAI Scripts';

CREATE TABLE `player_drop_template` (
  `guid` bigint(16) unsigned NOT NULL DEFAULT '0',
  `racemask` int(11) NOT NULL DEFAULT '-1',
  `classmask` int(11) NOT NULL DEFAULT '-1',
  `gender` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `item` int(11) unsigned NOT NULL DEFAULT '0',
  `chance` float unsigned NOT NULL DEFAULT '100',
  `mincount` int(11) unsigned NOT NULL DEFAULT '1',
  `maxcount` int(11) unsigned NOT NULL DEFAULT '1',
  `kminlvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `kmaxlvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `vminlvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `vmaxlvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `lvldiff` tinyint(3) NOT NULL DEFAULT '-70',
  `map` int(10) NOT NULL DEFAULT '-1',
  PRIMARY KEY (`guid`,`racemask`,`classmask`,`gender`,`item`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '20558', '0', '1', '1', '0', '0', '0', '0', '-70', '-1');
INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '20559', '0', '1', '1', '0', '0', '0', '0', '-70', '-1');
INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '20560', '21', '1', '1', '0', '0', '0', '0', '-70', '-1');
INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '29024', '0', '1', '1', '0', '0', '0', '0', '-70', '-1');
INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '42425', '21', '1', '1', '0', '0', '0', '0', '-70', '-1');
INSERT INTO `player_drop_template` VALUES ('0', '-1', '-1', '0', '43589', '21', '1', '1', '0', '0', '0', '0', '-70', '-1');
