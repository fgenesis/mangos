DROP TABLE IF EXISTS hidden_rating;
CREATE TABLE IF NOT EXISTS hidden_rating (
    guid INT(11) UNSIGNED NOT NULL,
    rating2 INT(10) UNSIGNED NOT NULL,
    rating3 INT(10) UNSIGNED NOT NULL,
    rating5 INT(10) UNSIGNED NOT NULL,
    PRIMARY KEY  (guid)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8;