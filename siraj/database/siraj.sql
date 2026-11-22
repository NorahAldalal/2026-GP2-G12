-- phpMyAdmin SQL Dump
-- version 4.9.0.1
-- https://www.phpmyadmin.net/
--
-- PHP Version: 7.2.22

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET AUTOCOMMIT = 0;
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `siraj`
--

-- --------------------------------------------------------

--
-- بنية الجدول `admin`
--

CREATE TABLE `admin` (
  `AdminID` int(11) NOT NULL,
  `AdminCode` varchar(50) DEFAULT NULL,
  `AdminName` varchar(100) NOT NULL,
  `Email` varchar(255) NOT NULL,
  `Password` varchar(255) NOT NULL,
  `CreatedAt` datetime DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- إرجاع أو استيراد بيانات الجدول `admin`
--


-- --------------------------------------------------------

--
-- بنية الجدول `area`
--

CREATE TABLE `area` (
  `AreaID` int(11) NOT NULL,
  `AreaName` varchar(150) NOT NULL,
  `Latitude` decimal(10,7) NOT NULL,
  `Longitude` decimal(10,7) NOT NULL,
  `Pollution_level` enum('Low','Medium','High') DEFAULT 'Low',
  `Pollution_Reduction` decimal(10,2) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- إرجاع أو استيراد بيانات الجدول `area`
--

INSERT INTO `area` (`AreaID`, `AreaName`, `Latitude`, `Longitude`, `Pollution_level`, `Pollution_Reduction`) VALUES
(1, 'Downtown', '24.6877000', '46.7219000', 'High', '0.00'),
(2, 'Al Hamra District', '24.6950000', '46.7350000', 'High', '0.00'),
(3, 'Industrial Zone', '24.6700000', '46.7100000', 'High', '0.00'),
(4, 'Residential North', '24.7050000', '46.7150000', 'High', '0.00');

-- --------------------------------------------------------

--
-- بنية الجدول `employee`
--

CREATE TABLE `employee` (
  `EmployeeID` int(11) NOT NULL,
  `EmployeeCode` varchar(50) DEFAULT NULL,
  `EmployeeName` varchar(100) NOT NULL,
  `Email` varchar(255) NOT NULL,
  `Password` varchar(255) NOT NULL,
  `AreaID` int(11) DEFAULT NULL,
  `AdminID` int(11) DEFAULT NULL,
  `CreatedAt` datetime DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- إرجاع أو استيراد بيانات الجدول `employee`
--


-- --------------------------------------------------------

--
-- بنية الجدول `experiment_cycle`
--

CREATE TABLE `experiment_cycle` (
  `CycleID` int(11) NOT NULL,
  `ExperimentID` int(11) NOT NULL,
  `CycleNumber` tinyint(4) NOT NULL,
  `OffHz` decimal(10,4) NOT NULL,
  `FullHz` decimal(10,4) NOT NULL,
  `ModelHz` decimal(10,4) NOT NULL,
  `FullExcessHz` decimal(10,4) NOT NULL,
  `ModelExcessHz` decimal(10,4) NOT NULL,
  `MeanModelPwm` decimal(8,4) NOT NULL,
  `MeanFullRoadLux` decimal(8,4) DEFAULT NULL,
  `MeanModelRoadLux` decimal(8,4) DEFAULT NULL,
  `ReductionPct` decimal(10,6) NOT NULL,
  `BackgroundDriftPct` decimal(8,5) DEFAULT NULL,
  `OffCvPct` decimal(8,5) DEFAULT NULL,
  `FullCvPct` decimal(8,5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- إرجاع أو استيراد بيانات الجدول `experiment_cycle`
--

INSERT INTO `experiment_cycle` (`CycleID`, `ExperimentID`, `CycleNumber`, `OffHz`, `FullHz`, `ModelHz`, `FullExcessHz`, `ModelExcessHz`, `MeanModelPwm`, `MeanFullRoadLux`, `MeanModelRoadLux`, `ReductionPct`, `BackgroundDriftPct`, `OffCvPct`, `FullCvPct`) VALUES
(1, 1, 1, '199.9000', '543.0625', '365.2667', '343.1625', '165.3667', '48.0000', '9.2000', '4.2000', '51.810974', '0.07782', '0.49878', '0.32807'),
(2, 1, 2, '199.7444', '543.1667', '365.1667', '343.4222', '165.4222', '48.0000', '9.2000', '4.2000', '51.831241', '0.30038', '0.65416', '0.28743'),
(3, 1, 3, '200.1333', '542.9889', '426.8889', '342.8556', '226.7556', '70.7778', '9.2000', '6.8556', '33.862657', '0.46636', '0.65754', '0.06979');

-- --------------------------------------------------------

--
-- بنية الجدول `experiment_result`
--

CREATE TABLE `experiment_result` (
  `ExperimentID` int(11) NOT NULL,
  `RunID` varchar(64) NOT NULL,
  `AreaID` int(11) NOT NULL,
  `ExperimentDate` date NOT NULL,
  `StartedAt` datetime DEFAULT NULL,
  `EndedAt` datetime DEFAULT NULL,
  `SourceTimezone` varchar(50) NOT NULL DEFAULT 'Asia/Riyadh',
  `ScenarioRecorded` varchar(50) NOT NULL,
  `ScenarioDescription` varchar(255) NOT NULL,
  `ModelID` varchar(100) NOT NULL,
  `MetricType` varchar(100) NOT NULL DEFAULT 'background_subtracted_sensor_signal_reduction',
  `ReferenceMode` varchar(50) NOT NULL DEFAULT 'full_pwm_100',
  `ReductionPct` decimal(10,6) DEFAULT NULL,
  `NumericalStatus` varchar(60) NOT NULL,
  `PhysicalVerificationStatus` varchar(60) NOT NULL,
  `RetainedModelSamples` int(11) DEFAULT NULL,
  `MeanAppliedPwm` decimal(8,4) DEFAULT NULL,
  `MinAppliedPwm` decimal(8,4) DEFAULT NULL,
  `MaxAppliedPwm` decimal(8,4) DEFAULT NULL,
  `SourceCsvFile` varchar(255) DEFAULT NULL,
  `SourceJsonFile` varchar(255) DEFAULT NULL,
  `Notes` text DEFAULT NULL,
  `CreatedAt` datetime DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- إرجاع أو استيراد بيانات الجدول `experiment_result`
--

INSERT INTO `experiment_result` (`ExperimentID`, `RunID`, `AreaID`, `ExperimentDate`, `StartedAt`, `EndedAt`, `SourceTimezone`, `ScenarioRecorded`, `ScenarioDescription`, `ModelID`, `MetricType`, `ReferenceMode`, `ReductionPct`, `NumericalStatus`, `PhysicalVerificationStatus`, `RetainedModelSamples`, `MeanAppliedPwm`, `MinAppliedPwm`, `MaxAppliedPwm`, `SourceCsvFile`, `SourceJsonFile`, `Notes`, `CreatedAt`) VALUES
(1, '20261120_001517_6f7840', 1, '2026-11-20', '2026-11-20 00:15:18', '2026-11-20 00:27:18', 'Asia/Riyadh', 'empty', 'Real-world operation with mixed pedestrian and vehicle activity.', 'siraj-cleaned782-synthetic134-v1', 'background_subtracted_sensor_signal_reduction', 'full_pwm_100', '45.840040', 'NUMERIC_PASS', 'PENDING_PHYSICAL_CHECK', 27, '55.5926', '48.0000', '100.0000', 'siraj_model_20261120_001517_6f7840.csv', 'siraj_model_20261120_001517_6f7840.json', 'Relative change in background-subtracted light-sensor signal compared with 100% PWM reference across 3 cycles. Numerical checks passed; physical sky attribution check pending.', '2026-11-21 19:05:02');

-- --------------------------------------------------------

--
-- بنية الجدول `lamp`
--

CREATE TABLE `lamp` (
  `LampID` int(11) NOT NULL,
  `Status` enum('on','off') DEFAULT 'on',
  `Lux_Value` decimal(8,2) DEFAULT 0.00,
  `AreaID` int(11) NOT NULL,
  `offset_lat` decimal(10,7) DEFAULT 0.0000000,
  `offset_lng` decimal(10,7) DEFAULT 0.0000000
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- إرجاع أو استيراد بيانات الجدول `lamp`
--

INSERT INTO `lamp` (`LampID`, `Status`, `Lux_Value`, `AreaID`, `offset_lat`, `offset_lng`) VALUES
(1, 'on', '150.00', 1, '0.0010000', '0.0012000'),
(2, 'off', '0.00', 1, '0.0020000', '-0.0999990'),
(3, 'on', '170.00', 1, '-0.0005000', '0.0020000'),
(4, 'on', '130.00', 2, '0.0016000', '0.0005000'),
(5, 'on', '150.00', 2, '-0.0010000', '0.9900000'),
(6, 'off', '0.00', 2, '0.0025000', '-0.0012000'),
(7, 'on', '110.00', 3, '0.0008000', '0.0010000'),
(8, 'on', '145.00', 3, '-0.0015000', '0.0022000'),
(9, 'off', '0.00', 3, '0.0018000', '-0.0005000'),
(10, 'on', '160.00', 4, '0.0012000', '0.0015000'),
(11, 'on', '135.00', 4, '-0.0008000', '0.0025000'),
(12, 'off', '0.00', 4, '0.0022000', '-0.0010000');

-- --------------------------------------------------------

--
-- بنية الجدول `lampreading`
--

CREATE TABLE `lampreading` (
  `readingID` int(11) NOT NULL,
  `LampID` int(11) NOT NULL,
  `ambientLight` decimal(8,2) DEFAULT NULL,
  `motionDetected` tinyint(1) DEFAULT 0,
  `readingTime` datetime DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- --------------------------------------------------------

--
-- بنية الجدول `report`
--

CREATE TABLE `report` (
  `ReportID` int(11) NOT NULL,
  `LampID` int(11) NOT NULL,
  `EmployeeID` int(11) NOT NULL,
  `Details` text NOT NULL,
  `Status` enum('pending','in_progress','resolved') DEFAULT 'pending',
  `CreatedAt` datetime DEFAULT current_timestamp(),
  `UpdatedAt` datetime DEFAULT current_timestamp() ON UPDATE current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- إرجاع أو استيراد بيانات الجدول `report`
--


-- --------------------------------------------------------

--
-- بنية الجدول `weekly_reading`
--

CREATE TABLE `weekly_reading` (
  `WeekID` int(11) NOT NULL,
  `AreaID` int(11) NOT NULL,
  `WeekStart` date NOT NULL,
  `WeekEnd` date NOT NULL,
  `AvgAmbient` decimal(8,2) DEFAULT 0.00,
  `AvgLux` decimal(8,2) DEFAULT 0.00,
  `ReadingCount` int(11) DEFAULT 0,
  `CreatedAt` datetime DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_general_ci;

--
-- إرجاع أو استيراد بيانات الجدول `weekly_reading`
--

INSERT INTO `weekly_reading` (`WeekID`, `AreaID`, `WeekStart`, `WeekEnd`, `AvgAmbient`, `AvgLux`, `ReadingCount`, `CreatedAt`) VALUES
(1, 1, '2026-03-16', '2026-03-22', '82.50', '148.20', 2016, '2026-04-12 13:09:24'),
(2, 1, '2026-03-23', '2026-03-29', '78.30', '152.10', 2016, '2026-04-12 13:09:24'),
(3, 1, '2026-03-30', '2026-04-05', '85.70', '145.80', 2016, '2026-04-12 13:09:24'),
(4, 1, '2026-04-06', '2026-04-12', '79.90', '150.40', 2016, '2026-04-12 13:09:24'),
(5, 2, '2026-03-16', '2026-03-22', '65.10', '128.50', 2016, '2026-04-12 13:09:24'),
(6, 2, '2026-03-23', '2026-03-29', '70.40', '131.20', 2016, '2026-04-12 13:09:24'),
(7, 2, '2026-03-30', '2026-04-05', '68.80', '135.60', 2016, '2026-04-12 13:09:24'),
(8, 2, '2026-04-06', '2026-04-12', '72.30', '129.80', 2016, '2026-04-12 13:09:24'),
(9, 3, '2026-03-16', '2026-03-22', '45.20', '108.40', 2016, '2026-04-12 13:09:24'),
(10, 3, '2026-03-23', '2026-03-29', '48.60', '112.30', 2016, '2026-04-12 13:09:24'),
(11, 3, '2026-03-30', '2026-04-05', '43.90', '106.70', 2016, '2026-04-12 13:09:24'),
(12, 3, '2026-04-06', '2026-04-12', '50.10', '114.20', 2016, '2026-04-12 13:09:24'),
(13, 4, '2026-03-16', '2026-03-22', '58.70', '132.10', 2016, '2026-04-12 13:09:24'),
(14, 4, '2026-03-23', '2026-03-29', '61.20', '135.40', 2016, '2026-04-12 13:09:24'),
(15, 4, '2026-03-30', '2026-04-05', '55.80', '130.60', 2016, '2026-04-12 13:09:24'),
(16, 4, '2026-04-06', '2026-04-12', '63.40', '137.90', 2016, '2026-04-12 13:09:24');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `admin`
--
ALTER TABLE `admin`
  ADD PRIMARY KEY (`AdminID`),
  ADD UNIQUE KEY `uq_admin_email` (`Email`);

--
-- Indexes for table `area`
--
ALTER TABLE `area`
  ADD PRIMARY KEY (`AreaID`);

--
-- Indexes for table `employee`
--
ALTER TABLE `employee`
  ADD PRIMARY KEY (`EmployeeID`),
  ADD UNIQUE KEY `uq_emp_email` (`Email`),
  ADD KEY `fk_emp_area` (`AreaID`),
  ADD KEY `fk_emp_admin` (`AdminID`);

--
-- Indexes for table `experiment_cycle`
--
ALTER TABLE `experiment_cycle`
  ADD PRIMARY KEY (`CycleID`),
  ADD UNIQUE KEY `uq_experiment_cycle` (`ExperimentID`,`CycleNumber`);

--
-- Indexes for table `experiment_result`
--
ALTER TABLE `experiment_result`
  ADD PRIMARY KEY (`ExperimentID`),
  ADD UNIQUE KEY `uq_experiment_run_id` (`RunID`),
  ADD KEY `fk_experiment_area` (`AreaID`);

--
-- Indexes for table `lamp`
--
ALTER TABLE `lamp`
  ADD PRIMARY KEY (`LampID`),
  ADD KEY `fk_lamp_area` (`AreaID`);

--
-- Indexes for table `lampreading`
--
ALTER TABLE `lampreading`
  ADD PRIMARY KEY (`readingID`),
  ADD KEY `fk_reading_lamp` (`LampID`);

--
-- Indexes for table `report`
--
ALTER TABLE `report`
  ADD PRIMARY KEY (`ReportID`),
  ADD KEY `fk_report_lamp` (`LampID`),
  ADD KEY `fk_report_emp` (`EmployeeID`);

--
-- Indexes for table `weekly_reading`
--
ALTER TABLE `weekly_reading`
  ADD PRIMARY KEY (`WeekID`),
  ADD UNIQUE KEY `unique_area_weekstart` (`AreaID`,`WeekStart`) USING BTREE,
  ADD KEY `AreaID` (`AreaID`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `admin`
--
ALTER TABLE `admin`
  MODIFY `AdminID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT for table `area`
--
ALTER TABLE `area`
  MODIFY `AreaID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT for table `employee`
--
ALTER TABLE `employee`
  MODIFY `EmployeeID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=16;

--
-- AUTO_INCREMENT for table `experiment_cycle`
--
ALTER TABLE `experiment_cycle`
  MODIFY `CycleID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `experiment_result`
--
ALTER TABLE `experiment_result`
  MODIFY `ExperimentID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT for table `lamp`
--
ALTER TABLE `lamp`
  MODIFY `LampID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=13;

--
-- AUTO_INCREMENT for table `lampreading`
--
ALTER TABLE `lampreading`
  MODIFY `readingID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `report`
--
ALTER TABLE `report`
  MODIFY `ReportID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=8;

--
-- AUTO_INCREMENT for table `weekly_reading`
--
ALTER TABLE `weekly_reading`
  MODIFY `WeekID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=17;

--
-- قيود الجداول المحفوظة
--

--
-- القيود للجدول `employee`
--
ALTER TABLE `employee`
  ADD CONSTRAINT `fk_emp_admin` FOREIGN KEY (`AdminID`) REFERENCES `admin` (`AdminID`) ON DELETE SET NULL,
  ADD CONSTRAINT `fk_emp_area` FOREIGN KEY (`AreaID`) REFERENCES `area` (`AreaID`) ON DELETE SET NULL;

--
-- القيود للجدول `experiment_cycle`
--
ALTER TABLE `experiment_cycle`
  ADD CONSTRAINT `fk_cycle_experiment` FOREIGN KEY (`ExperimentID`) REFERENCES `experiment_result` (`ExperimentID`) ON DELETE CASCADE;

--
-- القيود للجدول `experiment_result`
--
ALTER TABLE `experiment_result`
  ADD CONSTRAINT `fk_experiment_area` FOREIGN KEY (`AreaID`) REFERENCES `area` (`AreaID`) ON DELETE CASCADE;

--
-- القيود للجدول `lamp`
--
ALTER TABLE `lamp`
  ADD CONSTRAINT `fk_lamp_area` FOREIGN KEY (`AreaID`) REFERENCES `area` (`AreaID`) ON DELETE CASCADE;

--
-- القيود للجدول `lampreading`
--
ALTER TABLE `lampreading`
  ADD CONSTRAINT `fk_reading_lamp` FOREIGN KEY (`LampID`) REFERENCES `lamp` (`LampID`) ON DELETE CASCADE;

--
-- القيود للجدول `report`
--
ALTER TABLE `report`
  ADD CONSTRAINT `fk_report_emp` FOREIGN KEY (`EmployeeID`) REFERENCES `employee` (`EmployeeID`) ON DELETE CASCADE,
  ADD CONSTRAINT `fk_report_lamp` FOREIGN KEY (`LampID`) REFERENCES `lamp` (`LampID`) ON DELETE CASCADE;

--
-- القيود للجدول `weekly_reading`
--
ALTER TABLE `weekly_reading`
  ADD CONSTRAINT `weekly_reading_ibfk_1` FOREIGN KEY (`AreaID`) REFERENCES `area` (`AreaID`);
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
