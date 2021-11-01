CREATE DATABASE zelezarna;

CREATE USER 'zelezarnauser'@'%' IDENTIFIED BY 'zelezarna21';
GRANT ALL PRIVILEGES ON zelezarna.* TO 'zelezarnauser'@'%';
FLUSH PRIVILEGES;

-------------------------------------------------------
-- Energy data
-------------------------------------------------------

DELIMITER $$

CREATE TABLE zelezarna.energy_data (
  `time` TIMESTAMP NOT NULL,
  `energy` double DEFAULT NULL,
  PRIMARY KEY (`time`),
  INDEX (`energy`)
);

CREATE FUNCTION zelezarna.upsert_energy(ptime BIGINT, pval DOUBLE)
RETURNS INT DETERMINISTIC 
BEGIN
        INSERT INTO energy_data
        VALUES (FROM_UNIXTIME(ptime), pval)
        ON DUPLICATE KEY UPDATE `time`=FROM_UNIXTIME(ptime), energy=pval;
        RETURN 1;
END;

CREATE PROCEDURE zelezarna.get_energy(pfrom BIGINT, pto BIGINT)
BEGIN
        (SELECT time, UNIX_TIMESTAMP(time) AS unixtime, energy FROM zelezarna.energy_data
        WHERE UNIX_TIMESTAMP(time)<pfrom AND energy IS NOT NULL ORDER BY time DESC LIMIT 1)

        UNION 

        (SELECT time, UNIX_TIMESTAMP(time) AS unixtime, energy FROM zelezarna.energy_data
        WHERE UNIX_TIMESTAMP(time)>=pfrom AND UNIX_TIMESTAMP(time)<=pto AND energy IS NOT NULL)

        UNION

        (SELECT time, UNIX_TIMESTAMP(time) AS unixtime, energy FROM zelezarna.energy_data
        WHERE UNIX_TIMESTAMP(time)>pto AND energy IS NOT NULL ORDER BY time ASC LIMIT 1)
        ;
END;

$$

-------------------------------------------------------
-- Production data
-------------------------------------------------------

DELIMITER $$

CREATE TABLE zelezarna.production_data (
  `time` TIMESTAMP NOT NULL,
  `production` double DEFAULT NULL,
  PRIMARY KEY (`time`)
);

CREATE FUNCTION zelezarna.upsert_production(ptime BIGINT, pval DOUBLE)
RETURNS INT DETERMINISTIC 
BEGIN
        INSERT INTO production_data
        VALUES (FROM_UNIXTIME(ptime), pval)
        ON DUPLICATE KEY UPDATE `time`=FROM_UNIXTIME(ptime), production=pval;
        RETURN 1;
END;

CREATE PROCEDURE zelezarna.get_production(pfrom BIGINT, pto BIGINT)
BEGIN
        SELECT time, UNIX_TIMESTAMP(time) AS unixtime, production FROM zelezarna.production_data
        WHERE UNIX_TIMESTAMP(time)>pfrom AND UNIX_TIMESTAMP(time)<=pto AND production IS NOT NULL;
END;

$$
