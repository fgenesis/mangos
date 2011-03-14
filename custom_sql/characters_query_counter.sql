CREATE TABLE `query_counter` (
  `starttime` bigint(20) unsigned NOT NULL,
  `str` longtext NOT NULL,
  `count` bigint(20) unsigned NOT NULL DEFAULT '0',
  `totaltime` bigint(20) unsigned NOT NULL DEFAULT '0',
  KEY `idx_starttime` (`starttime`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
