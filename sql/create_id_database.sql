drop database RandomIDDatabase;

-- Create the database
CREATE DATABASE IF NOT EXISTS RandomIDDatabase;

-- Use the newly created database
USE RandomIDDatabase;

-- Create the table with a random integer column
CREATE TABLE IF NOT EXISTS RandomNumbers (
    id INT AUTO_INCREMENT PRIMARY KEY,
    random_value INT NOT NULL
);

-- Insert 1 million rows with random numbers between 0 and 100
DELIMITER $$

CREATE PROCEDURE InsertRandomNumbers()
BEGIN
    DECLARE counter INT DEFAULT 1;
    WHILE counter <= 100000000 DO
        INSERT INTO RandomNumbers (random_value) VALUES (FLOOR(RAND() * 101));
        SET counter = counter + 1;
    END WHILE;
END$$

DELIMITER ;

-- Call the procedure to insert the rows
CALL InsertRandomNumbers();

-- Optional: Drop the procedure after execution
DROP PROCEDURE IF EXISTS InsertRandomNumbers;
