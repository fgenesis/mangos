CREATE TABLE `query_counter` (
  `starttime` bigint(20) unsigned NOT NULL,
  `db_ident` varchar(1) NOT NULL DEFAULT 'U',
  `str` longtext NOT NULL,
  `count` bigint(20) unsigned NOT NULL DEFAULT '0',
  KEY `idx_starttime` (`starttime`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
