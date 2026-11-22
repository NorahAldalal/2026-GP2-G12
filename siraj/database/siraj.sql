-- phpMyAdmin SQL Dump
-- version 4.9.0.1
-- https://www.phpmyadmin.net/
--
-- Host: sql206.infinityfree.com
-- Generation Time: 22 نوفمبر 2026 الساعة 14:44
-- إصدار الخادم: 11.4.14-MariaDB
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
-- Database: `if0_43446175_siraj`
--

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

INSERT INTO `employee` (`EmployeeID`, `EmployeeCode`, `EmployeeName`, `Email`, `Password`, `AreaID`, `AdminID`, `CreatedAt`) VALUES
(1, '11', 'Deemah', 'demohato@gmail.com', '$2y$10$fkoQGHEJMHr2He9Oqw.5UODmPAQNNKA.qIV7OUoKPXEA67GzpRqfG', 1, 1, '2026-04-12 12:35:39'),
(2, '22', 'Aseel', 'AseelAbdulaziz771@gmail.com', '$2y$10$KQRJnltDGDy5NEfD1Y7q5.VHrfPNH8OaPrcdW2kW2RnHniw8rg4.C', 2, 1, '2026-04-12 12:35:39'),
(3, '33', 'Reema', 'Alnajimreema@gmail.com', '$2y$12$SYUykgzGSpasGNkKQlEKAuPlDBPCBNliEGqVFllcfrf8Qx3nGGlcq', 4, 1, '2026-04-12 12:35:39'),
(4, '55', 'Norah', 'Norahaldlal@gmail.com', '$2y$12$WLiO.k7BR1fv6vYjhl2KsOxuo5acpboBor3JlUSTCxdDRm.ragszC', 1, 1, '2026-04-12 12:45:13'),
(15, '66', 'Aseel', 'asas5357@gmail.com', '$2y$10$4Gveou1SPCY.TS3szHP8Ku/eIhXfExPdFwDq/JlGzgBC5Tb/vqc76', 2, 1, '2026-05-17 09:58:50'),
(17, '01', 'Employee', 'employee@siraj.city', '$2y$12$bQUt6uEhIBCjsBf6IZczm.jk15JFJvO6l880YoQf.g3//vvEJ8.pa', 1, 1, '2026-11-22 11:39:30');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `employee`
--
ALTER TABLE `employee`
  ADD PRIMARY KEY (`EmployeeID`),
  ADD UNIQUE KEY `uq_emp_email` (`Email`),
  ADD KEY `fk_emp_area` (`AreaID`),
  ADD KEY `fk_emp_admin` (`AdminID`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `employee`
--
ALTER TABLE `employee`
  MODIFY `EmployeeID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=18;

--
-- قيود الجداول المحفوظة
--

--
-- القيود للجدول `employee`
--
ALTER TABLE `employee`
  ADD CONSTRAINT `fk_emp_admin` FOREIGN KEY (`AdminID`) REFERENCES `admin` (`AdminID`) ON DELETE SET NULL,
  ADD CONSTRAINT `fk_emp_area` FOREIGN KEY (`AreaID`) REFERENCES `area` (`AreaID`) ON DELETE SET NULL;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
