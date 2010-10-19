SET FOREIGN_KEY_CHECKS=0;

CREATE TABLE `blocked_ips` (
  `ip` varchar(20) COLLATE utf8_bin NOT NULL,
  `recorded` bigint(20) unsigned NOT NULL,
  `rec_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `cnt` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ip`,`recorded`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;


-- ----------------------------
-- Table structure for punish_action
-- ----------------------------
CREATE TABLE `punish_action` (
  `what` varchar(30) CHARACTER SET utf8 NOT NULL,
  `action` varchar(30) CHARACTER SET utf8 NOT NULL,
  `arg` longtext CHARACTER SET utf8 NOT NULL,
  PRIMARY KEY (`what`,`action`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;

-- ----------------------------
-- Table structure for punish_value
-- ----------------------------
CREATE TABLE `punish_value` (
  `value` int(11) NOT NULL,
  `action` varchar(30) NOT NULL,
  `arg` longtext NOT NULL,
  PRIMARY KEY (`value`,`action`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records 
-- ----------------------------
INSERT INTO `punish_action` VALUES ('english', 'points', '100');
INSERT INTO `punish_action` VALUES ('spam', 'points', '500');
INSERT INTO `punish_action` VALUES ('insult', 'points', '3000');
INSERT INTO `punish_action` VALUES ('hax', 'points', '10000');
INSERT INTO `punish_action` VALUES ('chartrade', 'points', '4000');
INSERT INTO `punish_action` VALUES ('english', 'notify', '|cffFF7005In public channels use english only, thanks.');
INSERT INTO `punish_action` VALUES ('wrongchat', 'points', '50');
INSERT INTO `punish_action` VALUES ('wrongchat', 'notify', '|cffFF7005You were using the wrong chat, please refer to the chat/channel rules. (To avoid spamming channels with unrelated topics) ');
INSERT INTO `punish_action` VALUES ('chartrade', 'notify', '|cffFF7005Character trading is not allowed. Don\'t do it.');
INSERT INTO `punish_action` VALUES ('test', 'notify', '|cff00FF00THIS IS A TEST!|r');
INSERT INTO `punish_action` VALUES ('english', 'chanmute', '20');
INSERT INTO `punish_action` VALUES ('english2', 'chanmute', '1440');
INSERT INTO `punish_action` VALUES ('english2', 'notify', '|cffFF7005You have been warned. In public channels use english only -- is this so hard to understand?');
INSERT INTO `punish_action` VALUES ('english2', 'points', '150');
INSERT INTO `punish_action` VALUES ('haxnoban', 'points', '9600');
INSERT INTO `punish_action` VALUES ('wrongchat2', 'chanmute', '240');
INSERT INTO `punish_action` VALUES ('wrongchat2', 'notify', '|cffFF7005You were using the wrong chat AGAIN, go read the chat/channel rules NOW.');
INSERT INTO `punish_action` VALUES ('wrongchat', 'chanmute', '10');
INSERT INTO `punish_action` VALUES ('wrongchat2', 'points', '80');
INSERT INTO `punish_action` VALUES ('wrongchat3', 'chanmute', '3000');
INSERT INTO `punish_action` VALUES ('wrongchat3', 'notify', '|cffFF7005You have repeatedly violated the channel/chat rules, time to be quiet. You have been told to read the rules, looks like you did not. Shame.');
INSERT INTO `punish_action` VALUES ('wrongchat3', 'points', '150');
INSERT INTO `punish_value` VALUES ('300', 'mute', '10');
INSERT INTO `punish_value` VALUES ('500', 'mute', '30');
INSERT INTO `punish_value` VALUES ('800', 'mute', '300');
INSERT INTO `punish_value` VALUES ('2000', 'ban', '3h');
INSERT INTO `punish_value` VALUES ('3000', 'ban', '3d');
INSERT INTO `punish_value` VALUES ('5000', 'ban', '7d');
INSERT INTO `punish_value` VALUES ('10000', 'ban', '-1');
INSERT INTO `punish_value` VALUES ('8000', 'ban', '14d');
INSERT INTO `punish_value` VALUES ('2500', 'ban', '1d');
INSERT INTO `punish_value` VALUES ('1200', 'ban', '1h');
