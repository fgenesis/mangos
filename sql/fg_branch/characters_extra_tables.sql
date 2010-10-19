CREATE TABLE `allowed_gm_accounts` (
  `id` int(11) unsigned NOT NULL,
  `comment` varchar(100) COLLATE utf8_bin NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;

CREATE TABLE `channels_special` (
  `name` varchar(20) CHARACTER SET utf8 NOT NULL,
  `no_notify` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `unowned` tinyint(1) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin;

CREATE TABLE `character_extra` (
  `guid` int(11) unsigned NOT NULL,
  `xp_multi_kill` float unsigned NOT NULL DEFAULT '1',
  `xp_multi_quest` float unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `character_myinfo` (
  `guid` bigint(20) NOT NULL DEFAULT '0',
  `forbidden` tinyint(3) NOT NULL DEFAULT '0',
  `msg` longtext,
  `story` longtext,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `network_usage_in` (
  `starttime` bigint(20) unsigned NOT NULL,
  `opcode` int(11) unsigned NOT NULL,
  `packets` bigint(20) unsigned NOT NULL DEFAULT '0',
  `bytes` bigint(20) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`starttime`,`opcode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `network_usage_out` (
  `starttime` bigint(20) unsigned NOT NULL,
  `opcode` int(11) unsigned NOT NULL,
  `packets` bigint(20) unsigned NOT NULL DEFAULT '0',
  `bytes` bigint(20) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`starttime`,`opcode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `vp_chars` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `traits_id` int(11) unsigned NOT NULL,
  `name` varchar(12) NOT NULL,
  `race` int(11) unsigned NOT NULL,
  `class` int(11) unsigned NOT NULL,
  `lvl` int(11) NOT NULL,
  `xp` int(11) NOT NULL,
  `honor` int(11) NOT NULL,
  `quests` int(11) NOT NULL,
  `info` longtext NOT NULL,
  `planned_logout` int(20) unsigned NOT NULL,
  `last_logout` int(20) unsigned NOT NULL,
  `zone` int(11) unsigned NOT NULL,
  `gender` tinyint(1) unsigned NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `map` int(11) unsigned NOT NULL,
  `guild` varchar(100) NOT NULL DEFAULT '',
  `guid` int(11) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3414 DEFAULT CHARSET=utf8;

CREATE TABLE `vp_chars_update` (
  `id` int(11) unsigned NOT NULL,
  `info` longtext,
  `guild` varchar(100) DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `vp_online` (
  `id` int(11) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `vp_times` (
  `hour` int(11) unsigned NOT NULL DEFAULT '0',
  `min` int(11) unsigned NOT NULL DEFAULT '0',
  `max` int(11) unsigned DEFAULT '0',
  PRIMARY KEY (`hour`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
